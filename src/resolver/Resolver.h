#pragma once

#include <string>
using std::string;

#include <memory>
using std::unique_ptr;

#include <map>
#include <utility>
#include <chrono>

#include "singleton.h"
#include "drivers/Driver.h"
#include "ResolverException.h"
#include "transport/Transport.h"
#include "drivers/modules/AsyncHttpClient.h"

/**
 * @brief Forward declaration for singleton driver type define
 */
class Resolver;
using resolver = singleton<Resolver>;

typedef struct ResolverRequest {
    uint32_t id = -1;
    int type = -1;
    CDriverCfg::CfgUniqId_t db_id = -1;
    ClientInfo client_info;
    string data;
    std::chrono::system_clock::time_point req_start;
    bool is_done = false;
    CDriver::SResult_t result;

    ResolverRequest();
    void parse(const RecvData &recv_data);
} ResolverRequest;

/**
 * @brief Resolver Handler
 */

 class ResolverHandler {
public:
    virtual void make_http_request(Resolver* resolver,
                                   const ResolverRequest &request,
                                   const HttpRequest &http_request) = 0;
};

/**
 * @brief Resolver class
 */
class Resolver :
    public TransportHandler,
    public AsyncHttpClientHandler,
    public ResolverHandler {

public:
    Resolver();
    ~Resolver() = default;

    bool configure();

    /* TransportHandler */
    virtual void on_data_received(Transport *transport,
                               const RecvData &recv_data) override;

    /* AsyncHttpClientHandler */
    virtual void on_http_response_received(AsyncHttpClient *http_client,
                                   const HttpResponse &response) override;

    /* ResolverInterface */
    virtual void make_http_request(Resolver* resolver,
                                   const ResolverRequest &request,
                                   const HttpRequest &http_request) override;

    static void send_reply(const ResolverRequest &request);

private:
    void resolve(ResolverRequest &request);

    void parse_response(const HttpResponse &response,
                        ResolverRequest &request);

    void handle_request_is_done(const ResolverRequest &request, CDriver *driver);

    // Databases type defines
    using Database_t = std::map<CDriverCfg::CfgUniqId_t, unique_ptr<CDriver> >;
    static bool loadResolveDrivers(Database_t & db);

    static void send_provisional_reply(const ResolverRequest &request);
    static void send_tagged_reply(const ResolverRequest &request);
    static void send_json_reply(const ResolverRequest &request);

    static void send_error_reply(const ResolverRequest &request,
                                 const ECErrorId code,
                                 const string &description);
    static void send_tagged_error_reply(const ResolverRequest &request,
                                        const ECErrorId code,
                                        const string &description);
    static void send_json_error_reply(const ResolverRequest &request,
                                      const ECErrorId code,
                                      const string &description);

    Database_t mDriversMap;
    mutex mDriversMutex;

    AsyncHttpClient http_client;
    map<uint32_t, ResolverRequest> waiting_requests;
};

