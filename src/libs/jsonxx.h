#ifndef SERVER_SRC_LIBS_JSONXX_H
#define SERVER_SRC_LIBS_JSONXX_H

#include <memory>
#include <type_traits>
#include <string>
using std::string;

#include "cJSON.h"

/**
 * @brief The JSON wrapper class
 */
class jsonxx
{
  public:
    class item
    {
      private:
        cJSON * mObjItem;   // item object based

        // Internal helpers
        bool isNull() const   { return cJSON_NULL == mObjItem->type; }
        bool isBool() const   { return (cJSON_True == mObjItem->type) ||
                                       (cJSON_False == mObjItem->type); }
        bool isNumber() const { return cJSON_Number == mObjItem->type; }
        bool isString() const { return cJSON_String == mObjItem->type; }
        bool isList() const   { return cJSON_Array == mObjItem->type; }
        bool isMap() const    { return cJSON_Object == mObjItem->type; }

      public:
        explicit item(cJSON * root, const char * value);
        ~item() = default;

        template<class T,
        typename std::enable_if<std::is_integral<T>::value>::type* = nullptr>
        explicit operator T() const;

        template<class T,
        typename std::enable_if<std::is_floating_point<T>::value>::type* = nullptr>
        explicit operator T() const;

        explicit operator bool() const;
        explicit operator string() const;
    };

  private:
    cJSON * mData;                // JSON root object
    std::unique_ptr<item> mItem;  // JSON item object

  public:
    // does not allowed object copy procedures
    jsonxx(const jsonxx & obj) = delete;
    jsonxx & operator=(const jsonxx & obj) = delete;

    explicit jsonxx(const char * data);
    explicit jsonxx(const string & data);
    ~jsonxx();

    item & operator[](const char * value);
};

/**
 * @brief JSON item class cast operator for integral values
 *        (char, int/uint, long/ulong, int8_t/uint8_t, etc.)
 */
template<class T,
typename std::enable_if<std::is_integral<T>::value>::type*>
inline jsonxx::item::operator T() const
{
  T rv = 0;
  if(isNumber())
  {
    rv = static_cast<T> (mObjItem->valueint);
  }
  return rv;
}

/**
 * @brief JSON item class cast operator for floating values
 *        (float, double, long double)
 */
template<class T,
typename std::enable_if<std::is_floating_point<T>::value>::type*>
inline jsonxx::item::operator T() const
{
  T rv = 0.0;
  if(isNumber())
  {
    rv = static_cast<T> (mObjItem->valuedouble);
  }
  return rv;
}

/**
 * @brief JSON item class cast operator for boolean values
 */
inline jsonxx::item::operator bool() const
{
  bool rv = false;
  if(isBool() && (cJSON_True == mObjItem->type))
  {
    rv = true;
  }
  return rv;
}

/**
 * @brief JSON item class cast operator for basic string values
 *
 * @note also remove first and last quotes if required
 */
inline jsonxx::item::operator string() const
{
  string rv;
  if (isString())
  {
    rv = mObjItem->valuestring;

    // remove quotes
    if (0 == rv.compare(0, 1, "\""))
    {
      rv.erase(rv.begin());
    }
    if (0 == rv.compare(rv.size()-1, 1, "\""))
    {
      rv.erase(rv.end() - 1);
    }
  }
  return rv;
}

#endif // SERVER_SRC_LIBS_JSONXX_H
