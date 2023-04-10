#include <sstream>
#include <cstring>

#include "log.h"
#include "resolver/Resolver.h"
#include "CnamHttpDriver.h"
#include "JsonHelpers.h"

#include <type_traits>

/**************************************************************
 * Configuration implementation
***************************************************************/

void CCnamHttpDriverCfg::TemplateProcessor::parse_url_template(string &url)
{
    //split url to the parts
    auto s = url.data();
    enum {
        ST_NORMAL = 0,
        ST_PLACEHOLDER
    } state = ST_NORMAL;

    size_t i, pos = 0;

    for(i = 0; i < url.size(); i++)
    {
        char c = s[i];
        switch(state) {
        case ST_NORMAL:
            switch(c) {
            case '{':
                //new placeholder
                if(i > pos) {
                    //save previous static part
                    uri_parts.emplace_back(
                        UriPart::URI_PART_STRING,
                        url.substr(pos, i - pos));
                }
                pos = i+1;
                state = ST_PLACEHOLDER;
                break;
            case '}':
                throw std::runtime_error("unexpected '}' in the url template");
            default:
                break;
            } //switch(c)
            break;
        case ST_PLACEHOLDER:
            switch(c) {
            case '}':
                //placeholder closed
                if(1 > (i - pos)) {
                    throw std::runtime_error("empty placeholder in the url template");
                }
                uri_parts.emplace_back(
                    UriPart::URI_PART_PLACEHOLDER,
                    url.substr(pos, i - pos));
                pos = i+1;
                state = ST_NORMAL;
                break;
            case '{':
                throw std::runtime_error("unexpected '{' in the url template");
            default:
                break;
            } //switch(c)
            break;
        }
    }

    if(ST_PLACEHOLDER == state) {
        throw std::runtime_error("unclosed placeholder in the url template");
    }

    if(i > pos) {
        //save tail static part
        uri_parts.emplace_back(
            UriPart::URI_PART_STRING,
            url.substr(pos, i - pos));
    }

    /*for(auto &p: uri_parts) {
        dbg("uri_part %d %s", p.type, p.data.data());
    }*/
}

string CCnamHttpDriverCfg::TemplateProcessor::get_url(cJSON *j) const
{
    string ret;
    for(const auto &p: uri_parts) {
        switch(p.type) {
        case UriPart::URI_PART_STRING:
            ret += p.data;
            break;
        case UriPart::URI_PART_PLACEHOLDER:
            auto item = cJSON_GetObjectItem(j, p.data.data());
            if(!item)
                throw CDriver::error(
                    "missed required key '%s' in request",
                    p.data.data());
            switch(item->type) {
            case cJSON_String:
                ret += item->valuestring;
                break;
            case cJSON_Number:
                ret += std::to_string(item->valueint);
                break;
            case cJSON_True:
                ret += '1';
                break;
            case cJSON_False:
                ret += '0';
                break;
            default:
                throw CDriver::error(
                    "unsupported hash item type %d by key '%s'",
                    item->type, p.data.data());
            }
            break;
        }
    }
    return ret;
}
/**
 * @brief Driver configuration constructor
 *
 * @param[in] data    The raw configuration data
 */
CCnamHttpDriverCfg::CCnamHttpDriverCfg(const CDriverCfg::RawConfig_t & data)
  : CDriverCfg(data)
{
#define set_str_var(var) var = getJsonValueByKey<string>(j, #var)
    if (CDriverCfg::ECONFIG_DATA_AS_JSON_STRING != CDriverCfg::getFormatType())
        throw error(getLabel(), "unsupported config format");

    try {
        std::unique_ptr<cJSON, void(*)(cJSON*)> jptr(
            cJSON_Parse(data["parameters"].c_str()),
            cJSON_Delete);
        if(!jptr)
            throw error(getLabel(), "failed to parse json");
        auto &j = *jptr;

        timeout = getJsonValueByKey<CfgTimeout_t>(j, "timeout");

        set_str_var(url);
        template_processor.parse_url_template(url);

    } catch (std::exception & e) {
        throw error(getLabel(), e.what());
    }
#undef set_str_var
}

/**************************************************************
 * Driver implementation
***************************************************************/
/**
 * @brief Driver constructor
 *
 * @param[in] data  The raw configuration data
 */
CCnamHttpDriver::CCnamHttpDriver(const CDriverCfg::RawConfig_t & data)
  : CDriver(ECDriverId::ERESOLVER_DIRVER_CNAM_HTTP, "CNAM HTTP"),
    cfg(data)
{}

/**
 * @brief Show drive information
 */
void CCnamHttpDriver::showInfo() const
{
    info("[%u/%s] '%s' driver => url:<%s>, timeout:%u",
        cfg.getUniqId(),
        cfg.getLabel(), getName(),\
        cfg.url.c_str(),
        cfg.timeout);
}

/**
 * @brief Executing resolving procedure
 */
void CCnamHttpDriver::resolve(ResolverRequest &request,
                         Resolver *resolver,
                         ResolverDelegate *delegate) const {

    //parse inData as json
    std::unique_ptr<cJSON, void(*)(cJSON*)> request_json(
        cJSON_Parse(request.data.data()), cJSON_Delete);
    if(!request_json)
        throw CDriver::error("failed to parse request json");
    if(request_json->type != cJSON_Object)
        throw CDriver::error("expected JSON object in request");

    //replace placeholders in dstURL using data from json
    string dstURL = cfg.template_processor.get_url(request_json.get());

    HttpRequest http_request;
    http_request.id = request.id;
    http_request.method = GET;
    http_request.url = dstURL.c_str();

    http_request.verify_ssl = false;
    http_request.auth_type = ECAuth::NONE;
    http_request.timeout_ms = cfg.timeout;
    http_request.headers = { "Content-Type: application/json" };

    if (delegate != nullptr)
        delegate->make_http_request(resolver, request, http_request);
}

void CCnamHttpDriver::parse(const string &data, ResolverRequest &request) const {
    request.result.rawData = data;

    std::unique_ptr<cJSON, void(*)(cJSON*)> j(
        cJSON_Parse(data.data()),cJSON_Delete);
    if(!j)
        throw CDriver::error("failed to parse reply JSON");
    if(j->type != cJSON_Object)
        throw CDriver::error("expected JSON object in reply");

    auto response_json = cJSON_CreateObject();
    cJSON_AddItemReferenceToObject(response_json, "response", j.get());
    char *s = cJSON_PrintUnformatted(response_json);
    cJSON_Delete(response_json);

    request.result.rawData = s;
    free(s);
}
