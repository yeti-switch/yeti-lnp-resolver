#pragma once

#include <string>
#include <stdexcept>

#include "libs/cJSON.h"

using std::string;

template<typename T>
T getJsonValue(const cJSON &j)
{
    if(std::is_integral<T>::value) {
        if(j.type!=cJSON_Number)
            throw std::runtime_error("getJsonKey unexpected type for integral return");
        return static_cast<T>(j.valueint);
    } else if(std::is_floating_point<T>::value) {
        if(j.type!=cJSON_Number)
            throw std::runtime_error("getJsonKey unexpected type for floating point return");
        return static_cast<T>(j.valuedouble);
    } else {
        throw std::runtime_error("getJsonKey: unsupported dst type");
    }
}

template<> string getJsonValue(const cJSON &j);

template<typename T>
T getJsonValueByKey(cJSON &j, const char *key)
{
    if(j.type!=cJSON_Object)
        throw std::runtime_error("getJsonValueByKey passed non-object item");
    auto i = cJSON_GetObjectItem(&j, key);
    if(!i)
        throw std::runtime_error("getJsonValueByKey key not found: ");
    return getJsonValue<T>(*i);
}
