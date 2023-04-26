#include "Resolver.h"
#include "cfg.h"
#include "cache.h"
#include "drivers/Driver.h"
#include "drivers/DriverConfig.h"
#include "statistics/prometheus/prometheus_exporter.h"

#include <pqxx/pqxx>
#include <sys/time.h>

#define TAGGED_REQ_VERSION 0
#define TAGGED_PDU_HDR_SIZE 7
#define TAGGED_ERR_RESPONSE_HDR_SIZE 6

#define CNAM_REQ_VERSION 1
#define CNAM_HDR_SIZE 10
#define CNAM_RESPONSE_HDR_SIZE 8

static const char * sLoadLNPConfigSTMT = "SELECT * FROM load_lnp_databases()";

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
    //Mutex required to proper processing SIGHUP signal
    guard(mDriversMutex);
    mDriversMap.swap(dbMap);
  }

  return rv;
}

/**
 * @brief Transport delegate func
 */
void Resolver::data_received(Transport *transopot, const RecvData &recv_data) {
    ResolverRequest request;

    try
    {
        prepare_request(recv_data, request);
        resolve(request);
    } catch(const string & e) {
        err("got string exception: %s",e.c_str());

        string reply;
        prepare_error_reply(request, ECErrorId::GENERAL_ERROR, e, reply);
        transport::instance()->send_data(reply, request.client_info);

    } catch(const CResolverError & e) {
        err("got resolve exception: <%u> %s", static_cast<uint>(e.code()), e.what());

        string reply;
        prepare_error_reply(request, e.code(), e.what(), reply);
        transport::instance()->send_data(reply, request.client_info);
    }
}

void Resolver::resolve(ResolverRequest &request) {

    //Mutex required to proper processing SIGHUP signal
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

    try
    {
        driver->requests_count_increment();
        driver->resolve(request, this, this);
    }
    catch(const CDriver::error &e) {
        driver->requests_failed_increment();
        throw CResolverError(ECErrorId::DRIVER_RESOLVING_ERROR, e.what());
    }
    catch(const AsyncHttpClient::error &e) {
        driver->requests_failed_increment();
        throw CResolverError(ECErrorId::GENERAL_RESOLVING_ERROR, e.what());
    }
    catch(...) {
        driver->requests_failed_increment();
        throw CResolverError(ECErrorId::GENERAL_RESOLVING_ERROR,
                         "unknown resovling exception");
    }

    if (request.is_done)
        handle_request_is_done(request, driver);
}

/* ResolverDelegate */
void Resolver::make_http_request(Resolver* resolver,
                                 const ResolverRequest &request,
                                 const HttpRequest &http_request) {

    if (http_client == nullptr) {
        http_client = std::make_unique<AsyncHttpClient>();
        http_client->set_delegate(this);
    }

    waiting_requests[request.id] = std::move(request);
    http_client->make_request(http_request);
}

/**
 * @brief AsyncHttpClient delegate func
 */
void Resolver::response_received(AsyncHttpClient *http_client, const HttpResponse &response) {

    auto it = waiting_requests.find(response.id);
    if (it == waiting_requests.end()) {
        err("request not found");
        return;
    }

    ResolverRequest request = std::move(it->second);
    waiting_requests.erase(it);

    try
    {
        parse_response(response, request);
    } catch(const string & e){
        err("got string exception: %s", e.c_str());

        string reply;
        prepare_error_reply(request, ECErrorId::GENERAL_ERROR, e, reply);
        transport::instance()->send_data(reply, request.client_info);

    } catch(const CResolverError & e){
        err("got resolve exception: <%u> %s", static_cast<uint>(e.code()), e.what());

        string reply;
        prepare_error_reply(request, e.code(), e.what(), reply);
        transport::instance()->send_data(reply, request.client_info);
    }

    if (waiting_requests.empty() && this->http_client != nullptr) {
        this->http_client = nullptr;
    }
}

void Resolver::parse_response(const HttpResponse &response,
                              ResolverRequest &request) {

    //Mutex required to proper processing SIGHUP signal
    guard(mDriversMutex);

    auto mapItem = mDriversMap.find(request.db_id);
    if (mapItem == mDriversMap.end()) {
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

    try
    {
        driver->parse(response.data, request);
        request.is_done = true;
    }
    catch (const CDriver::error &e) {
        driver->requests_failed_increment();
        throw CResolverError(ECErrorId::DRIVER_RESOLVING_ERROR, e.what());
    }
    catch (...) {
        driver->requests_failed_increment();
        throw CResolverError(ECErrorId::GENERAL_RESOLVING_ERROR,
                         "unknown resovling exception");
    }

    if (request.is_done)
        handle_request_is_done(request, driver);
}

void Resolver::handle_request_is_done(const ResolverRequest &request, CDriver *driver) {

    // metrics
    if (driver != nullptr) {
        timeval req_end;
        timeval req_diff;
        uint64_t req_diff_millis;

        gettimeofday(&req_end, nullptr);
        timersub(&req_end, &request.req_start, &req_diff);
        req_diff_millis = req_diff.tv_sec * 1000 + req_diff.tv_usec / 1000;

        if(driver->getDriverType() == CDriver::DriverTypeTagged) {
            dbg("Resolved (by '%s/%d'): %s -> %s (tag: '%s') [in %ld.%06ld seconds]",
                driver->getName(), driver->getUniqueId(),
                request.data.c_str(),
                request.result.localRoutingNumber.c_str(),
                request.result.localRoutingTag.c_str(),
                req_diff.tv_sec, req_diff.tv_usec);
        } else {
            dbg("Resolved (by '%s/%d'): %s -> %s [in %ld.%06ld seconds]",
                driver->getName(), driver->getUniqueId(),
                request.data.data(),
                request.result.rawData.data(),
                req_diff.tv_sec, req_diff.tv_usec);
        }

        driver->requests_finished_increment(req_diff_millis);
    }

    // cache result
    if(driver->getDriverType() == CDriver::DriverTypeTagged)
        lnp_cache::instance()->sync(
            new cache_entry(driver->getUniqueId(), request.data, request.result));

    // reply to client
    string reply;
    prepare_reply(request, reply);
    transport::instance()->send_data(reply, request.client_info);
}

/* Static functions */

void Resolver::prepare_request(const RecvData &recv_data, ResolverRequest &out) {

    /* common req layout:
    *    4 byte - request id
    *    1 byte - database id
    *    1 byte - request type
    *      * 0: lnp_resolve_tagged
    *      * 1: lnp_resolve_cnam
    */

    auto & msg = recv_data.data;
    auto & len = recv_data.length;

    out.type = TAGGED_REQ_VERSION;
    out.client_info = std::move(recv_data.client_info);
    out.is_done = false;
    gettimeofday(&out.req_start, nullptr);

    if(len < 6) {
        throw CResolverError(ECErrorId::PSQL_INVALID_REQUEST, "request too small");
    }

    out.id = *(uint32_t *)msg;
    out.db_id = msg[4];
    out.type = msg[5];
    dbg("db_id %d, type: %d", out.db_id, out.type);

    int data_offset, data_len;

    //check type
    switch(out.type) {
    case TAGGED_REQ_VERSION:
        /* tagged req layout:
        *    4 byte - request id
        *    1 byte - database id
        *    1 byte - request type = 0
        *    1 byte - number length
        *    n bytes - number data
        */
        if(len < TAGGED_PDU_HDR_SIZE) {
            throw CResolverError(ECErrorId::PSQL_INVALID_REQUEST, "request too small");
        }
        data_offset = TAGGED_PDU_HDR_SIZE;
        data_len = msg[6];
        break;
    case CNAM_REQ_VERSION:
        /* cnam req layout:
        *    4 byte - request id
        *    1 byte - database id
        *    1 byte - request type = 1
        *    4 bytes - json length
        *    n bytes - json data
        */
        if(len < CNAM_HDR_SIZE) {
            throw CResolverError(ECErrorId::PSQL_INVALID_REQUEST, "request too small");
        }
        data_offset = CNAM_HDR_SIZE;
        data_len = *(uint32_t *)(msg+6);
        break;
    default:
        out.type = TAGGED_REQ_VERSION; //force tagged reply
        throw CResolverError(ECErrorId::PSQL_INVALID_REQUEST, "uknown request type");
    }

    if((data_offset+data_len) > len) {
        err("malformed request: too big data_len");
        throw CResolverError(ECErrorId::PSQL_INVALID_REQUEST, "malformed request");
    }

    out.data = string(msg+data_offset,static_cast<size_t>(data_len));
    dbg("prepare request: db_id:%d, type:%d, data:%s", out.db_id, out.type, out.data.c_str());
}

void Resolver::prepare_reply(const ResolverRequest &request,
                            string &out) {
    switch(request.type) {
    case TAGGED_REQ_VERSION:
        prepare_tagged_reply(request, out);
        break;
    case CNAM_REQ_VERSION:
        prepare_json_reply(request, out);
        break;
    }
}

void Resolver::prepare_tagged_reply(const ResolverRequest &request,
                                    string &out) {
    // compose header
    char header[TAGGED_PDU_HDR_SIZE];
    memset(header, '\0', TAGGED_PDU_HDR_SIZE);
    *(uint32_t *)header = request.id;
    header[4]= char(ECErrorId::NO_ERROR);
    header[5]= char(request.result.localRoutingNumber.size()
        + request.result.localRoutingTag.size());
    header[6]= char(request.result.localRoutingNumber.size());

    // compose result
    out = string(header, TAGGED_PDU_HDR_SIZE)
        + request.result.localRoutingNumber
        + request.result.localRoutingTag;
}

void Resolver::prepare_json_reply(const ResolverRequest &request,
                                  string &out) {
    // compose header
    char header[CNAM_RESPONSE_HDR_SIZE];
    memset(header, '\0', CNAM_RESPONSE_HDR_SIZE);
    *(uint32_t *)header = request.id;
    *(uint32_t *)(header+4) = request.result.rawData.size();

    // compose result
    out = string(header, CNAM_RESPONSE_HDR_SIZE) + request.result.rawData;
}

void Resolver::prepare_error_reply(const ResolverRequest &request,
                                   const ECErrorId code,
                                   const string &description,
                                   string &out) {
    switch(request.type) {
    case TAGGED_REQ_VERSION:
        prepare_tagged_error_reply(request, code, description, out);
        break;
    case CNAM_REQ_VERSION:
        prepare_json_error_reply(request, code, description, out);
        break;
    }
}

void Resolver::prepare_tagged_error_reply(const ResolverRequest &request,
                                          const ECErrorId code,
                                          const string &s,
                                          string &out) {
    // compose header
    char header[TAGGED_ERR_RESPONSE_HDR_SIZE];
    memset(header, '\0', TAGGED_ERR_RESPONSE_HDR_SIZE);
    *(uint32_t *)header = request.id;
    header[4]= static_cast<char>(code);
    header[5]= static_cast<char>(s.size());

    // compose result
    out = string(header, TAGGED_ERR_RESPONSE_HDR_SIZE) + s;
}

void Resolver::prepare_json_error_reply(const ResolverRequest &request,
                                        const ECErrorId code,
                                        const string &s,
                                        string &out) {
    // compose json
    string json("{\"error\":{\"code\":");
    json += std::to_string((int)code);
    json += ",\"reason\":\"";
    json += s;
    json += "\"}}";

    // compose header
    char header[CNAM_RESPONSE_HDR_SIZE];
    memset(header, '\0', CNAM_RESPONSE_HDR_SIZE);

    *(uint32_t *)header = request.id;
    *(uint32_t *)(header+4) = json.size();

    // compose result
    out = string(header, CNAM_RESPONSE_HDR_SIZE) + json;
}
