#include "JsonHelpers.h"

template<>
string getJsonValue(const cJSON &j)
{
    string ret;
    if(j.type!=cJSON_String)
        throw std::runtime_error("getJsonKey unexpected type for string return");
    ret = string(j.valuestring);
    if (0 == ret.compare(0, 1, "\""))
        ret.erase(ret.begin());
    if (0 == ret.compare(ret.size()-1, 1, "\""))
        ret.erase(ret.end() - 1);
    return ret;
}
