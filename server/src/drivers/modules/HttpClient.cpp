#include "HttpClient.h"

// Static member as flag about internal library initialization
bool CHttpClient::sInitialize = false;

/*
 * Class constructor
 */
CHttpClient::CHttpClient()
  : mCtx(nullptr), mHdrList(NULL)
{
  mErrBuffer[0] = '\0';

  if (false == sInitialize)
  {
    // initialize global environment
    if (curl_global_init(CURL_GLOBAL_ALL))
      throw error("global environment initializing error");

    sInitialize = true;
  }

  // create request context
  mCtx = curl_easy_init();
  if (!mCtx)
    throw error("context creation error");
}

/*
 * Class destructor
 */
CHttpClient::~CHttpClient()
{
    if (NULL != mHdrList)
    {
        curl_slist_free_all(mHdrList);
    }

    if (nullptr != mCtx)
    {
        curl_easy_cleanup(mCtx);
    }
}

/*
 * Set up SSL verification
 *
 * @param[in] onoff   The disable/enable flag
 * @return none
 *
 * @note Enabled by default
 */
void CHttpClient::setSSLVerification(bool onoff)
{
    const long verify = onoff ? 2L : 0L;

    if ((curl_easy_setopt(mCtx, CURLOPT_SSL_VERIFYPEER, verify) != CURLE_OK) ||
        (curl_easy_setopt(mCtx, CURLOPT_SSL_VERIFYHOST, verify) != CURLE_OK))
        throw error("SSL verification option error");
}

/*
 * Set up authorization method type
 *
 * @param[in] type  The authorization method type
 * @return none
 *
 * @note By default is ECAuth::BASIC
 */
void CHttpClient::setAuthType(ECAuth type)
{
    if (CURLE_OK != curl_easy_setopt(mCtx, CURLOPT_HTTPAUTH, type))
        throw error("authorization type handling error");
}

/*
 * Set up authorization data for requests
 *
 * @param[in] login   The authorization login/user name data
 * @param[in] pass    The authorization password data
 * @return none
 */
void CHttpClient::setAuthData(const char * login, const char * pass)
{
    if ((curl_easy_setopt(mCtx,  CURLOPT_USERNAME, login) != CURLE_OK) ||
        (curl_easy_setopt(mCtx,  CURLOPT_PASSWORD, pass)  != CURLE_OK))
        throw error("authorization data error");
}

/*
 * Overloaded method for authorization data updating
 *
 * @see CHttpClient::setAuthData
 */
void CHttpClient::setAuthData(const string & login, const string & pass)
{
    return setAuthData(login.c_str(), pass.c_str());
}

/*
 * Set up request timeout
 *
 * @param[in] timeout   The timeout value
 */
void CHttpClient::setTimeout(const uint32_t timeout)
{
    if ((curl_easy_setopt(mCtx, CURLOPT_TIMEOUT_MS, timeout) != CURLE_OK) ||
        (curl_easy_setopt(mCtx, CURLOPT_NOSIGNAL, 1L)        != CURLE_OK))
        throw error("set up timeout value error");
}

/*
 * Add custom headers to the request
 *
 * @param[in] header  The header string value
 * @return none
 */
void CHttpClient::setHeader(const char * header)
{
    Hdr_t * hdrList = curl_slist_append(mHdrList, header);
    if (!hdrList)
        throw error("error appending header to request");

    mHdrList = hdrList;
}

/*
 * Overloaded method for custom headers
 *
 * @see CHttpClient::setHeader
 */
void CHttpClient::setHeader(const string & header)
{
    return setHeader(header.c_str());
}

/*
 * Set up callback for writing received data in reply
 *
 * @param[in]  fn    The callback function for processing reply
 * @param[out] ptr   The pointer to buffer for saving reply data
 * @return none
 */
void CHttpClient::setWriteCallback(WriteFunc_t fn, void * ptr)
{
    if ((curl_easy_setopt(mCtx, CURLOPT_WRITEFUNCTION, fn)   != CURLE_OK) ||
        (curl_easy_setopt(mCtx, CURLOPT_WRITEDATA, ptr) != CURLE_OK))
        throw error("initializing write callback data error");
}

/*
 * Retrieve error string if occurred
 *
 * @return string value with error data
 */
const char * CHttpClient::getErrorStr() const
{
    return mErrBuffer;
}

/*
 * Method for executing HTTP request
 *
 * @param[in] url   The URL string for processing request
 * @return request code value
 */
CHttpClient::ECReqCode CHttpClient::perform(const char * url)
{
    if (curl_easy_setopt(mCtx, CURLOPT_URL, url) != CURLE_OK)
        throw error("URL processing error");

    if (mHdrList)
    {
        if (curl_easy_setopt(mCtx, CURLOPT_HTTPHEADER, mHdrList) != CURLE_OK)
            throw error("headers processing error");
    }

    if(curl_easy_setopt(mCtx, CURLOPT_ERRORBUFFER, mErrBuffer) != CURLE_OK)
        throw error("error buffer initializing error");

    CURLcode rc = curl_easy_perform(mCtx);

    return (CURLE_OK == rc) ? ECReqCode::OK : ECReqCode::FAIL;
}

/*
 * Overloaded method for executing HTTP request
 *
 * @see CHttpClient::perform
 */
CHttpClient::ECReqCode CHttpClient::perform(const string & url)
{
    return perform(url.c_str());
}
