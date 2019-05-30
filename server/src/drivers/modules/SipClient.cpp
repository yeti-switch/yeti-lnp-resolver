#include <stdint.h> // Required re library
#include "re.h"

#include "SipClient.h"

/* Predefined values */
const char * const sDefUserAgent = "Yeti SIP client";
const uint32_t     sDefTimeout   = 4000;  // timeout in ms

/* Internal usage library data */
static const uint32_t sGenericHashTableSize = 32; // power of 2
static const uint32_t sNameServersSize      = 16;
static struct sa sNameServers[sNameServersSize];
static struct dnsc * sNameServerClient   = nullptr;
static struct sip *  sSipStack           = nullptr;
static struct sipsess_sock * sSipSocket  = nullptr;

static mutex sMutex;

/**
 * @brief Helper class to destruct singletone instance
 */
class CSipClientDestructor
{
  private:
    CSipClient * mInstance;

  public:
    CSipClientDestructor() = default;
    ~CSipClientDestructor();
    void initialize(CSipClient * instance);
};

/* Initialize static members */
CSipClient * CSipClient::sInstance = nullptr;
CSipClientDestructor CSipClient::sDestructor;

/**
 * @brief Initialize helper class
 *
 * @param [in] instance The SIP client instance
 */
void CSipClientDestructor::initialize(CSipClient * instance)
{
  mInstance = instance;
}

/**
 * @brief Helper class destructor
 */
CSipClientDestructor::~CSipClientDestructor()
{
  if (mInstance)
  {
    guard(sMutex);

    // Stop library main loop
    mInstance->stop();

    // Free libre state
    libre_close();

    delete mInstance;

    mem_deref(sSipSocket);
    mem_deref(sSipStack);
    mem_deref(sNameServerClient);
  }
}

/**
 * @brief Internal handler to process reply message
 *
 * @param msg [in]    The SIP reply message
 * @param reply [out] The reply container to save parsed message
 *
 * @return status of the reply handling
 */
static bool replyHandler(const sip_msg * msg, CSipClient::SReplyData * reply)
{
  bool rv = false;

  do
  {
    // Check input arguments
    if ((nullptr == msg) || (nullptr == reply))
      break;

    // Accepted only moved permanently or temporarily
    if (! ((301 == msg->scode) || (302 == msg->scode)))
      break;
    reply->statusCode = msg->scode;

    // Process contact field
    const struct sip_hdr * contact = sip_msg_hdr(msg, SIP_HDR_CONTACT);
    if (nullptr == contact)
      break;

    // Extract raw contact field data
    struct sip_addr rawContactField;
    if (sip_addr_decode(&rawContactField, &contact->val))
      break;
    reply->rawContactData = string(rawContactField.uri.user.p,
                                   rawContactField.uri.user.l);

    rv = true;
  } while(0);

  return rv;
}

/**
 * @brief Get SIP client instance.
 *        Makes the instance initialization for the first call
 *
 * @param [in] userAgent  The User-Agent field for SIP request
 *
 * @return reference to client instance
 */
CSipClient & CSipClient::getInstance(const char * userAgent)
{
  if (sInstance)
  {
    // Instance already initialized
    return *sInstance;
  }

  guard(sMutex);

  // Create instance
  sInstance = new CSipClient;
  sDestructor.initialize(sInstance);

  // Initialize basic members
  sInstance->mUserAgent = userAgent;
  if (!sInstance->mUserAgent)
  {
    sInstance->mUserAgent = sDefUserAgent;
  }
  sInstance->mTimeout = sDefTimeout;
  sInstance->mHandler = replyHandler;

  // Initialize re library
  if (0 != libre_init())
  {
    throw error("internal initialization error");
  }

  // Fetch list of DNS server IP addresses
  uint32_t nameServersCapacity = sNameServersSize;
  if (0 != dns_srv_get(nullptr, 0, sNameServers, &nameServersCapacity))
  {
    throw error("unable to get DNS servers");
  }

  // Initialize DNS client
  if (0 != dnsc_alloc(&sNameServerClient, nullptr,
                      sNameServers, nameServersCapacity))
  {
    throw error("unable to create DNS client");
  }

  // Create SIP stack instance
  if (0 != sip_alloc(&sSipStack, sNameServerClient, sGenericHashTableSize,
                     sGenericHashTableSize, sGenericHashTableSize,
                     userAgent, nullptr, nullptr))
  {
    throw error("unable to create SIP stack");
  }

  // Fetch local IP address
  struct sa socketAddr;
  if (0 != net_default_source_addr_get(AF_INET, &socketAddr))
  {
    throw error("unable to get default local address");
  }

  // Listen on random port
  sa_set_port(&socketAddr, 0);

  // Add supported SIP transports
  if (0 != sip_transp_add(sSipStack, SIP_TRANSP_UDP, &socketAddr))
  {
    throw error("unable to add supported SIP transport");
  }

  // Create SIP session socket
  if (0 != sipsess_listen(&sSipSocket, sSipStack,
                          sGenericHashTableSize, nullptr, nullptr))
  {
    throw error("unable to create SIP session socket");
  }

  // Start library main loop
  sInstance->start();

  return *sInstance;
}

/**
 * @brief Thread runner
 */
void CSipClient::run()
{
  set_name("SIP client");
  re_main(nullptr);
}

/**
 * @brief Thread stop handler
 */
void CSipClient::on_stop()
{
  // Wait for pending transactions to finish
  sipsess_close_all(sSipSocket);
  sip_close(sSipStack, false);

  // Stop libre main loop
  re_cancel();
}

/**
 * @brief Setter for Contact field
 *
 * @param [in] data  The value to field setting
 */
void CSipClient::setContactData(const char * data)
{
  if (data)
  {
    sInstance->mContactField = data;
  }
}

/**
 * @brief Setter for From field
 *
 * @param [in] name The name value for From field
 * @param [in] uri  The URI value for From field
 */
void CSipClient::setFromData(const char * name, const char * uri)
{
  if(name && uri)
  {
    sInstance->mFromName = name;
    sInstance->mFromUri  = uri;
  }
}

/**
 * @brief Setter for SIP client reply timeout
 *
 * @param timeout   The timeout value
 */
void CSipClient::setTimeout(const uint32_t timeout)
{
  if (0 != timeout)
  {
    sInstance->mTimeout = timeout;
  }
  else
  {
    sInstance->mTimeout = sDefTimeout;
  }
}

/**
 * @brief Executing SIP request (INVITE)
 *
 * @param [in] uri     The request URI data
 * @param [out] reply  The contend used for saving response
 *
 * @return execution result code
 */
CSipClient::ECCode CSipClient::perform(const char * uri, SReplyData & reply)
{
  if (nullptr == uri)
  {
    throw error("not specified SIP request URI");
  }

  struct sipsess * session = nullptr;
  auto closeHandler = [](int err, const struct sip_msg * msg, void * arg)
  {
    static_cast<void> (err);
    SReplyData * reply = static_cast<SReplyData *>(arg);
    if (sInstance->mHandler(msg, reply))
    {
      reply->processed.set(true);
    }
  };

  int rv = sipsess_connect(&session, sSipSocket, uri,
                           sInstance->mFromName,
                           sInstance->mFromUri,
                           sInstance->mContactField,
                           nullptr, 0, "", nullptr,
                           nullptr, nullptr, false,
                           nullptr, nullptr, nullptr,
                           nullptr, nullptr, nullptr,
                           closeHandler, &reply, nullptr);
  if (0 != rv)
  {
    throw error("unable to perform SIP request");
  }

  // Wait on response
  rv = reply.processed.wait_for_to(sInstance->mTimeout);
  if(!rv)
  {
    // Close all SIP sessions in the case timeout
    sipsess_close_all(sSipSocket);
  }

  // Terminate session
  mem_deref(session);

  return rv ? ECCode::OK : ECCode::FAIL;
}

/**
 * @brief Overloaded method for executing SIP request (INVITE)
 *
 * @see CSipClient::perform
 */
CSipClient::ECCode CSipClient::perform(const string & uri, SReplyData & reply)
{
  return perform(uri.c_str(), reply);
}
