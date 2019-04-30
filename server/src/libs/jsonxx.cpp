#include <stdexcept>
#include "jsonxx.h"

/*
 * JSON item constructor
 */
jsonxx::item::item(cJSON * root, const char * value)
{
  if (!root)
  {
    throw std::runtime_error("jsonxx: item parsing error");
  }

  if (cJSON_Object != root->type)
  {
    throw std::runtime_error("jsonxx: item parsing internal error");
  }

  mObjItem = cJSON_GetObjectItem(root, value);
}

/*
 * JSON basic class constructor
 */
jsonxx::jsonxx(const char * data)
  : mItem(nullptr)
{
  if (!data)
  {
    throw std::runtime_error("jsonxx: invalid data for parsing");
  }

  mData = cJSON_Parse(data);
  if (!mData)
  {
    throw std::runtime_error("jsonxx: data parsing error");
  }
}

/*
 * JSON basic class constructor
 */
jsonxx::jsonxx(const string & data)
  : mItem(nullptr)
{
  mData = cJSON_Parse(data.c_str());
  if (!mData)
  {
    throw std::runtime_error("jsonxx: data parsing error");
  }
}

/*
 * JSON basic class destructor
 */
jsonxx::~jsonxx()
{
  if (mData)
  {
    cJSON_Delete(mData);
  }
}

/*
 * JSON basic class operator to set up item
 * for data retrieving and future casting
 *
 * @note item release actions do not required
 *       before using with different values
 */
const jsonxx::item & jsonxx::operator[] (const char * value)
{
  if (!value)
  {
    throw std::runtime_error("jsonxx: invalid item value");
  }

  mItem.reset(new item(mData, value));

  return *(mItem.get());
}


