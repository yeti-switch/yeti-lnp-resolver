#include <sstream>
#include <cstring>

#include "log.h"
#include "drivers/modules/HttpClient.h"
#include "HttpCoureAnqDriver.h"
#include "JsonHelpers.h"

#include <type_traits>

/**************************************************************
 * Implementation helpers
***************************************************************/
/**
 * @brief Callback function for processing HTTP response
 */
size_t static write_func(void * ptr, size_t size, size_t nmemb, void * userdata)
{
    if (userdata) {
        std::string & s = *(reinterpret_cast<std::string *>(userdata));
        s.append(static_cast<char *> (ptr), (size * nmemb));
    }

    return (size * nmemb);
}

/**************************************************************
 * Configuration implementation
***************************************************************/

void CHttpCoureAnqDriverCfg::OperatorsMap_t::parse(cJSON &j)
{
    if(j.type!=cJSON_Object)
        throw std::runtime_error("OperatorsMap_t::parse: JSON object expected");
    bool has_default = false;
    for(auto c = j.child; c; c = c->next) {
        if(c->type != cJSON_Number) {
            error("unexpected operators_map value type %d by key '%s'. skip it", c->type, c->string);
            throw std::runtime_error("OperatorsMap_t::parse: unexpected operators_map value type");
        }
        if(0==strcmp(c->string, "default")) {
            if(has_default)
                throw std::runtime_error("OperatorsMap_t::parse: duplicate \"default\" key");
            has_default = true;
            default_value = getJsonValue<unsigned int>(*c);
            continue;
        }
        emplace(c->string, getJsonValue<unsigned int>(*c));
    }
    if(!has_default)
        throw std::runtime_error("OperatorsMap_t::parse: misssed \"default\" value mapping");
}

unsigned int CHttpCoureAnqDriverCfg::OperatorsMap_t::resolve(const string &operator_name) const
{
    auto it  = find(operator_name);
    if(it == end()) {
        dbg("operator '%s' not found. return default value %u",
            operator_name.data(), default_value);
        return default_value;
    }
    dbg("operator '%s' resolved to %u",
        operator_name.data(), it->second);
    return it->second;
}

/**
 * @brief Driver configuration constructor
 *
 * @param[in] data    The raw configuration data
 */
CHttpCoureAnqDriverCfg::CHttpCoureAnqDriverCfg(const CDriverCfg::RawConfig_t & data)
  : CDriverCfg(data)
{
#define set_str_var(var) var = getJsonValueByKey<string>(j, #var)
    if (CDriverCfg::ECONFIG_DATA_AS_JSON_STRING != CDriverCfg::getFormatType())
        throw error(getLabel(), "unsupported config format");

    try {
        //dbg("data: %s",data["parameters"].c_str());
        std::unique_ptr<cJSON, void(*)(cJSON*)> jptr(
            cJSON_Parse(data["parameters"].c_str()),
            cJSON_Delete);
        if(!jptr)
            throw error(getLabel(), "failed to parse json");
        auto &j = *jptr;

        set_str_var(base_url);
        set_str_var(username);
        set_str_var(password);
        set_str_var(country_code);
        timeout = getJsonValueByKey<CfgTimeout_t>(j, "timeout");

        //read operators map
        auto m = cJSON_GetObjectItem(&j, "operators_map");
        if(!m)
            throw error(getLabel(), "misssed operators_map field");
        switch(m->type) {
        case cJSON_String: {
            string operators_map_str = getJsonValue<string>(*m);
            //dbg("operators_map: '%s'", operators_map_str.data());
            std::unique_ptr<cJSON, void(*)(cJSON*)> operators_map_json(
                cJSON_Parse(operators_map_str.data()), cJSON_Delete);
            if(!operators_map_json)
                throw error(getLabel(), "failed to parse operators_map json");
            operators_map.parse(*operators_map_json);
        } break;
        case cJSON_Object:
            operators_map.parse(*m);
            break;
         default:
            throw error(getLabel(), "unexpected operators_map field type");
        }

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
CHttpCoureAnqDriver::CHttpCoureAnqDriver(const CDriverCfg::RawConfig_t & data)
  : CDriver(ECDriverId::ERESOLVER_DIRVER_HTTP_COUREANQ, "Coure ANQ"),
    cfg(data)
{
    std::ostringstream url;
    url << cfg.base_url <<
        "/api/json/LookUpNumber/GsmPortStatus?" <<
        "username=" << cfg.username <<
        "&password=" << cfg.password <<
        "&ServiceType=4" <<
        "&country=" << cfg.country_code <<
        "&numbersToLookUp=";

    url_prefix = url.str();
    //dbg("url_prefix: %s",url_prefix.data());
}

/**
 * @brief Show drive information
 */
void CHttpCoureAnqDriver::showInfo() const
{
    info("[%u/%s] '%s' driver => base_url:<%s>, timeout:%u, country_code:%s",
        cfg.getUniqId(),
        cfg.getLabel(), getName(),\
        cfg.base_url.c_str(),
        cfg.timeout,
        cfg.country_code.data());
}

/**
 * @brief Executing resolving procedure
 *
 * @param[in] inData      The source data for making resolving
 * @param[out] outResult  The structure for saving resolving result
 */
void CHttpCoureAnqDriver::resolve(const string & inData, SResult_t & outResult) const
{
    /*
    * Request URL:
    * # ~ http://anqtestapi.nms.com.ng/api/json/LookUpNumber/GsmPortStatus?
    * username=xxxxxx@anq.com&
    * password=xxcfc&
    * ServiceType=4&
    * country=234&
    * numbersToLookUp=08075597646
    */

    string dstURL = url_prefix + inData;
    //dbg("resolving by URL: '%s'", dstURL.c_str());

    string replyBuf;
    try {
        CHttpClient http;

        http.setSSLVerification(false);
        http.setAuthType(CHttpClient::ECAuth::NONE);
        http.setTimeout(cfg.timeout);
        http.setWriteCallback(write_func, &replyBuf);
        http.setHeader("Content-Type: application/json");

        if (CHttpClient::ECReqCode::OK != http.perform(dstURL)) {
            dbg("error on perform request: %s", http.getErrorStr());
            throw CDriver::error(http.getErrorStr());
        }
    } catch (CHttpClient::error & e) {
        throw CDriver::error(e.what());
    }

    /*
    Processing reply. Example:
     {
            "Result": [
             {
                 "CountryCode": "235",
                 "IsPorted": 1,
                 "Number": "2347068970633",
                 "OperatorMobileNumberCode": "49",
                 "TheOperator": "ETS",
                 "UniversalNumberFormat": "2347068970633"
             }
         ]
      }
    */

    //dbg("HTTP reply: %s", replyBuf.c_str());
    outResult.rawData = replyBuf;

    std::unique_ptr<cJSON, void(*)(cJSON*)> j(cJSON_Parse(replyBuf.data()),cJSON_Delete);
    if(!j)
        throw CDriver::error("failed to parse reply JSON");
    if(j->type != cJSON_Object)
        throw CDriver::error("expected JSON object in reply");

    auto result_j = cJSON_GetObjectItem(j.get(),"Result");
    if(!result_j)
        throw CDriver::error("no \"Result\" key in reply");
    if(result_j->type != cJSON_Array)
        throw CDriver::error("expected array in \"Result\" key in reply");
    if(!result_j->child)
        throw CDriver::error("empty array in \"Result\" key in reply");

    auto data_j = result_j->child;
    if(data_j->type != cJSON_Object)
        throw CDriver::error("expected object array in \"Result\" key in reply");

    //check if ported
    auto ported = getJsonValueByKey<int>(*data_j, "IsPorted");
    switch(ported) {
    case 1: //ported
        break;
    case 0: //not_ported
    case 2: //invalid
        dbg("number is not ported or invalid. return inData with empty tag");
        outResult.localRoutingNumber = inData;
        outResult.localRoutingTag.clear();
        return;
    default:
        warn("unexpected ported value %d in reply",ported);
        throw CDriver::error("unexpected ported \"IsPorted\" value in reply");
    }

    //!FIXME: should we use UniversalNumberFormat here ?
    outResult.localRoutingNumber = getJsonValueByKey<string>(*data_j, "Number");

    //get and resolve tag
    outResult.localRoutingTag = std::to_string(
        cfg.operators_map.resolve(
            getJsonValueByKey<string>(*data_j, "TheOperator")));
}
