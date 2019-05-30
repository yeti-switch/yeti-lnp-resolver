#ifndef SERVER_SRC_RESOLVEREXCEPTION_H_
#define SERVER_SRC_RESOLVEREXCEPTION_H_

#include <cstdint>
using std::uint8_t;

#include <string>
using std::string;

#include <stdexcept>
using std::runtime_error;

/**
 * @brief Resolver error codes list
 */
enum class ECErrorId : uint8_t
{
   NO_ERROR      = 0

  // General error codes - 0X
  ,GENERAL_ERROR  = 1

  // PostgreSQL request/reply errors - 1X
  ,PSQL_INVALID_REQUEST = 11

  // Resolving general and driers error - 2X
  ,GENERAL_RESOLVING_ERROR = 21
  ,DRIVER_RESOLVING_ERROR  = 22
};

/**
 * @brief Resolver errors class
 */
class CResolverError : public runtime_error
{
  private:
    ECErrorId mCode;    // It is part of responce message

  public:
    CResolverError(const ECErrorId code, const string & cause)
      : runtime_error(cause), mCode(code) { }

    const ECErrorId code() const { return mCode; }
};

#endif  /* SERVER_SRC_RESOLVEREXCEPTION_H_ */
