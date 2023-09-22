#include "Resolver.h"
#include "cfg.h"
#include "cache.h"
#include "drivers/Driver.h"
#include "drivers/DriverConfig.h"

#include <pqxx/pqxx>

#define TAGGED_REQ_VERSION 0
#define CNAM_REQ_VERSION 1

static const char * sLoadLNPConfigSTMT = "SELECT * FROM load_lnp_databases()";

#pragma pack(1)

struct hdr_common {
    uint32_t id; //request/reply id
};

struct req_hdr_common {
    hdr_common common;
    uint8_t database_id;
    uint8_t type; //0 - tagged, 1 - cnam
};

struct req_hdr_tagged {
    uint8_t number_size;
};

struct req_hdr_cnam {
    uint32_t json_size;
};

/* tagged req layout:
*    4 byte - request id
*    1 byte - database id
*    1 byte - request type = 0
*    1 byte - number length
*    n bytes - number data
*/
/* cnam req layout:
*    4 byte - request id
*    1 byte - database id
*    1 byte - request type = 1
*    4 bytes - json length
*    n bytes - json data
*/
struct req_hdr_cominbed {
    req_hdr_common hdr;
    union {
        req_hdr_tagged tagged;
        req_hdr_cnam cnam;
    } spec;
};

struct reply_hdr_tagged {
    hdr_common common;
    uint8_t code;
    uint8_t data_size;
    uint8_t lrn_size;
};

struct reply_hdr_tagged_err {
    hdr_common common;
    uint8_t code;
    uint8_t desc_size;
};

struct reply_hdr_cnam {
    hdr_common common;
    uint32_t json_size;
};

#pragma pack()

#define TAGGED_PDU_HDR_SIZE (sizeof(req_hdr_common) + sizeof(req_hdr_tagged))
#define CNAM_HDR_SIZE (sizeof(req_hdr_common) + sizeof(req_hdr_cnam))

ResolverRequest::ResolverRequest()
  : req_start(std::chrono::system_clock::now()),
    is_done(false)
{}

void ResolverRequest::parse(const RecvData &recv_data)
{
    client_info = std::move(recv_data.client_info);

    const auto &len = recv_data.length;

    if (len < sizeof(req_hdr_common)) {
        throw CResolverError(ECErrorId::PSQL_INVALID_REQUEST, "request is too small");
    }

    const auto &req =
        *reinterpret_cast<const req_hdr_cominbed *>(recv_data.data);

    id = req.hdr.common.id;
    db_id = req.hdr.database_id;
    type = req.hdr.type;

    dbg("db_id %d, type: %d", db_id, type);

    size_t data_offset, data_len;

    //check type
    switch(type) {
    case TAGGED_REQ_VERSION:
        if(len < TAGGED_PDU_HDR_SIZE) {
            throw CResolverError(ECErrorId::PSQL_INVALID_REQUEST, "request is too small");
        }
        data_offset = TAGGED_PDU_HDR_SIZE;
        data_len = req.spec.tagged.number_size;
        break;
    case CNAM_REQ_VERSION:
        if(len < CNAM_HDR_SIZE) {
            throw CResolverError(ECErrorId::PSQL_INVALID_REQUEST, "request is too small");
        }
        data_offset = CNAM_HDR_SIZE;
        data_len = req.spec.cnam.json_size;
        break;
    default:
        type = TAGGED_REQ_VERSION; //force tagged reply
        throw CResolverError(ECErrorId::PSQL_INVALID_REQUEST, "unknown request type");
    }

    if((data_offset+data_len) > len) {
        err("malformed request: too big data_len:%lu", data_len);
        throw CResolverError(ECErrorId::PSQL_INVALID_REQUEST, "malformed request");
    }

    data.assign(recv_data.data + data_offset, data_len);

    dbg("parsed request: db_id:%d, type:%d, data:%.*s",
        db_id, type, static_cast<int>(data.length()), data.c_str());
}

Resolver::Resolver()
  : http_client(this)
{}

/**
 * @brief Load resolver drivers configuration from database table
 *
 * @param[in,out] dbMap   The map of the successfully loaded drivers
 *
 * @return boolean value as a loading procedure status
 */
bool Resolver::loadResolveDrivers(Database_t & dbMap)
{
  // Status about successfully loaded
  // database and initialized resolvers map
  bool rv = false;

  // Try to load drivers from database configuration
  try
  {
    pqxx::result dbResult;
    pqxx::connection dbConn(cfg.db.get_conn_string());
    dbConn.set_variable("search_path", cfg.db.schema + ", public");

    pqxx::work t(dbConn);
    dbResult = t.exec(sLoadLNPConfigSTMT);

    for (pqxx::result::size_type i = 0; i < dbResult.size(); ++i)
    {
      const pqxx::row & dbResultRaw = dbResult[i];
      unique_ptr<CDriver> drv = CDriver::instantiate(dbResultRaw);
      if (drv)
      {
        dbMap.emplace(drv->getUniqueId(), std::move(drv));
      }
      else
      {
        warn("Not supported driver provided in the raw %lu", i);
      }
    }

    // Show information about loaded drivers
    info("Loaded %lu drivers with %s", dbMap.size(), CDriverCfg::getFormatStrType());
    for (const auto & i : dbMap)
    {
      i.second->showInfo();
    }
    rv = true;
  }
  catch (const pqxx::pqxx_exception & e)
  {
    // show pqxx exception messages
    err("pqxx_exception: %s ", e.base().what());
  }
  catch (const CDriverCfg::error & e)
  {
    // show driver configuration error (abort main process)
    err("Driver config [%s]: %s", e.getIdent(), e.what());
  }
  catch (...)
  {
    // general exception handler (abort main process)
    err("Unexpected driver loading exception");
  }

  return rv;
}

/**
 * @brief Root method to load driver configuration
 *
 * @return boolean status about loading result
 */
bool Resolver::configure()
{
  Database_t dbMap;
  bool rv = loadResolveDrivers(dbMap);
  if (rv)
  {
    //Mutex required for proper SIGHUP signal processing
    guard(mDriversMutex);
    mDriversMap.swap(dbMap);
  }

  return rv;
}

/**
 * @brief Transport handler func
 */
void Resolver::on_data_received(Transport *, const RecvData &recv_data)
{
    ResolverRequest request;

    try {
        request.parse(recv_data);
        send_provisional_reply(request);
        resolve(request);
    } catch(const string & e) {
        err("got string exception: %s",e.c_str());
        send_error_reply(request, ECErrorId::GENERAL_ERROR, e);
    } catch(const CResolverError & e) {
        err("got resolve exception: <%u> %s", static_cast<uint>(e.code()), e.what());
        send_error_reply(request, e.code(), e.what());
    }
}

void Resolver::resolve(ResolverRequest &request)
{

    //Mutex required for proper SIGHUP signal processing
    guard(mDriversMutex);

    auto mapItem = mDriversMap.find(request.db_id);
    if (mapItem == mDriversMap.end()) {
        throw CResolverError(ECErrorId::GENERAL_RESOLVING_ERROR, "unknown database id");
    }

    CDriver *driver = mapItem->second.get();

    // check db type
    if (driver->getDriverType() != request.type) {
        throw CResolverError(
            ECErrorId::GENERAL_RESOLVING_ERROR,
            "request type is unsupported");
    }

    try {
        driver->requests_count_increment();
        driver->resolve(request, this, this);
    } catch(const CDriver::error &e) {
        driver->requests_failed_increment();
        throw CResolverError(ECErrorId::DRIVER_RESOLVING_ERROR, e.what());
    } catch(const AsyncHttpClient::error &e) {
        driver->requests_failed_increment();
        throw CResolverError(ECErrorId::GENERAL_RESOLVING_ERROR, e.what());
    } catch(...) {
        driver->requests_failed_increment();
        throw CResolverError(ECErrorId::GENERAL_RESOLVING_ERROR,
                         "unknown resovling exception");
    }

    if (request.is_done)
        handle_request_is_done(request, driver);
}

/* ResolverHandler */
void Resolver::make_http_request(Resolver*,
                                 const ResolverRequest &request,
                                 const HttpRequest &http_request)
{
    waiting_requests.emplace(request.id, std::move(request));
    http_client.make_request(http_request);
}

/**
 * @brief AsyncHttpClient handler func
 */
void Resolver::on_http_response_received(AsyncHttpClient *, const HttpResponse &response)
{

    auto it = waiting_requests.find(response.id);
    if (it == waiting_requests.end()) {
        err("request not found");
        return;
    }

    ResolverRequest request(std::move(it->second));
    waiting_requests.erase(it);

    try {
        parse_response(response, request);
    } catch(const string & e) {
        err("got string exception: %s", e.c_str());

        send_error_reply(request, ECErrorId::GENERAL_ERROR, e);
    } catch(const CResolverError & e) {
        err("got resolve exception: <%u> %s", static_cast<uint>(e.code()), e.what());

        send_error_reply(request, e.code(), e.what());
    }
}

void Resolver::parse_response(const HttpResponse &response,
                              ResolverRequest &request)
{

    //Mutex required to proper processing SIGHUP signal
    guard(mDriversMutex);

    auto mapItem = mDriversMap.find(request.db_id);
    if(mapItem == mDriversMap.end()) {
        throw CResolverError(ECErrorId::GENERAL_RESOLVING_ERROR, "unknown database id");
    }

    CDriver *driver = mapItem->second.get();

    // check db type
    if(driver->getDriverType() != request.type) {
        throw CResolverError(
            ECErrorId::GENERAL_RESOLVING_ERROR,
            "request type is unsupported");
    }

    // check http response
    if (response.is_success == false) {
        dbg("http response error: %s", response.data.c_str());
        driver->requests_failed_increment();
        throw CResolverError(ECErrorId::GENERAL_RESOLVING_ERROR, response.data.c_str());
        return;
    }

    try {
        driver->parse(response.data, request);
        request.is_done = true;
    } catch (const CDriver::error &e) {
        driver->requests_failed_increment();
        throw CResolverError(ECErrorId::DRIVER_RESOLVING_ERROR, e.what());
    } catch (...) {
        driver->requests_failed_increment();
        throw CResolverError(ECErrorId::GENERAL_RESOLVING_ERROR,
                         "unknown resovling exception");
    }

    if (request.is_done)
        handle_request_is_done(request, driver);
}

void Resolver::handle_request_is_done(const ResolverRequest &request, CDriver *driver)
{

    // metrics
    if (driver != nullptr) {
        auto req_diff = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now() - request.req_start);

        if(driver->getDriverType() == CDriver::DriverTypeTagged) {
            dbg("Resolved (by '%s/%d'): %s -> %s (tag: '%s') [in %ld ms]",
                driver->getName(), driver->getUniqueId(),
                request.data.c_str(),
                request.result.localRoutingNumber.c_str(),
                request.result.localRoutingTag.c_str(),
                req_diff.count());
        } else {
            dbg("Resolved (by '%s/%d'): %s -> %s [in %ld ms]",
                driver->getName(), driver->getUniqueId(),
                request.data.data(),
                request.result.rawData.data(),
                req_diff.count());
        }

        driver->requests_finished_increment(req_diff.count());
    }

    // cache result
    if(driver->getDriverType() == CDriver::DriverTypeTagged)
        lnp_cache::instance()->sync(
            new cache_entry(driver->getUniqueId(), request.data, request.result));

    // reply to client
    send_reply(request);
}

void Resolver::send_provisional_reply(const ResolverRequest &request)
{
    transport::instance()->send_data(
        &request.id, sizeof(request.id), request.client_info);
}

void Resolver::send_reply(const ResolverRequest &request)
{
    switch(request.type) {
    case TAGGED_REQ_VERSION:
        send_tagged_reply(request);
        break;
    case CNAM_REQ_VERSION:
        send_json_reply(request);
        break;
    }
}

void Resolver::send_tagged_reply(const ResolverRequest &request)
{
    string buf;
    buf.resize(sizeof(reply_hdr_tagged));

    auto &reply = *reinterpret_cast<reply_hdr_tagged *>(&buf[0]);

    reply.common.id = request.id;
    reply.code = static_cast<typeof(reply.code)>(ECErrorId::NO_ERROR);
    reply.data_size =
        request.result.localRoutingNumber.size() +
        request.result.localRoutingTag.size();
    reply.lrn_size = request.result.localRoutingNumber.size();

    buf += request.result.localRoutingNumber;
    buf += request.result.localRoutingTag;

    transport::instance()->send_data(buf, request.client_info);
}

void Resolver::send_json_reply(const ResolverRequest &request)
{
    string buf;
    buf.resize(sizeof(reply_hdr_cnam));

    auto &reply = *reinterpret_cast<reply_hdr_cnam *>(&buf[0]);

    reply.common.id = request.id;
    reply.json_size = request.result.rawData.size();

    buf += request.result.rawData;

    transport::instance()->send_data(buf, request.client_info);
}

void Resolver::send_error_reply(const ResolverRequest &request,
                                const ECErrorId code,
                                const string &description)
{
    switch(request.type) {
    case TAGGED_REQ_VERSION:
        send_tagged_error_reply(request, code, description);
        break;
    case CNAM_REQ_VERSION:
        send_json_error_reply(request, code, description);
        break;
    }
}

void Resolver::send_tagged_error_reply(const ResolverRequest &request,
                                       const ECErrorId code,
                                       const string &description)
{
    string buf;
    buf.resize(sizeof(reply_hdr_tagged_err));

    auto &reply = *reinterpret_cast<reply_hdr_tagged_err *>(&buf[0]);

    reply.common.id = request.id;
    reply.code = static_cast<typeof(reply.code)>(code);
    reply.desc_size = static_cast<typeof(reply.desc_size)>(description.size());

    buf += description;

    transport::instance()->send_data(buf, request.client_info);
}

void Resolver::send_json_error_reply(const ResolverRequest &request,
                                     const ECErrorId code,
                                     const string &status)
{
    // compose json
    string json("{\"error\":{\"code\":");
    json += std::to_string((int)code);
    json += ",\"reason\":\"";
    json += status;
    json += "\"}}";

    string buf;
    buf.resize(sizeof(reply_hdr_cnam));

    auto &reply = *reinterpret_cast<reply_hdr_cnam *>(&buf[0]);

    reply.common.id = request.id;
    reply.json_size = json.size();

    buf += json;

    transport::instance()->send_data(buf, request.client_info);
}
