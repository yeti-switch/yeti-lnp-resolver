#ifndef SERVER_SRC_DRIVERS_MODULES_CSVCLIENT_H_
#define SERVER_SRC_DRIVERS_MODULES_CSVCLIENT_H_

#include <stdexcept>
using std::runtime_error;

#include <string>
using std::string;

#include <vector>
using std::vector;

#include <unordered_map>
using std::unordered_map;

#include "libs/fmterror.h"

/**
 * @brief CSV client class
 */
class CCsvClient
{
  public:
    // Client exception class
    class error : public runtime_error
    {
      public:
        template <typename ... Args>
        explicit error(const char * fmt, Args ... args) :
          runtime_error(fmterror(fmt, args ...).get()) { }
    };

    // Related type defines
    struct SRow
    {
        size_t id;
        vector<string> field;

        SRow(size_t i, vector<string> f)
        {
          id    = i;
          field = std::move(f);
        }
    };
    using row_t  = struct SRow;
    using hash_t = unordered_map<string, row_t>;

  private:
    hash_t mHash;   // in-memory hash table

  public:
    CCsvClient(const char *filePath, const uint8_t primaryField,
               const uint8_t fieldsNumberm);
    ~CCsvClient() = default;

    CCsvClient(const CCsvClient & cl)             = delete;
    CCsvClient & operator=(const CCsvClient & cl) = delete;

    const row_t * perform(const char * number);
    const row_t * perform(const string & number);

    size_t size() const { return mHash.size(); }
};

#endif /* SERVER_SRC_DRIVERS_MODULES_CSVCLIENT_H_ */
