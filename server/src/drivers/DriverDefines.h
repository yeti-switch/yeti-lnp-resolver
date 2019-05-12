#ifndef SERVER_SRC_DRIVERS_DRIVERDEFINES_H_
#define SERVER_SRC_DRIVERS_DRIVERDEFINES_H_

#include <array>

/**
 * @brief Internal driver unique identity
 */
enum class ECDriverId : uint8_t
{
   ERESOLVER_DRIVER_NULL           = 0
  ,ERESOLVER_DRIVER_SIP            = 1
  ,ERESOLVER_DRIVER_HTTP_THINQ     = 2
  ,ERESOLVER_DRIVER_MHASH_CSV      = 3
  ,ERESOLVER_DRIVER_HTTP_ALCAZAR   = 4
  ,ERESOLVER_DIRVER_HTTP_COUREANQ  = 5
  ,ERESOLVER_DRIVER_MAX_ID
};

/**
 * @brief An array with driver types, that depend on configuration format
 * @note numberId   The type used with yeti-web v1.7
 * @note stringId   The type used with yeti-web v1.8
 */
struct SDriverTypeMap_t
{
    ECDriverId    privateId;
    uint16_t      numberId;
    const char *  stringId;
};
static const std::array<SDriverTypeMap_t,
  (static_cast<long unsigned int> (ECDriverId::ERESOLVER_DRIVER_MAX_ID) - 1)>
sDriverTypeArray =
{{
     { ECDriverId::ERESOLVER_DRIVER_SIP,            1,  "Lnp::DatabaseSipRedirect" }
    ,{ ECDriverId::ERESOLVER_DRIVER_HTTP_THINQ,     2,  "Lnp::DatabaseThinq" }
    ,{ ECDriverId::ERESOLVER_DRIVER_MHASH_CSV,      3,  "Lnp::DatabaseCsv" }
    ,{ ECDriverId::ERESOLVER_DRIVER_HTTP_ALCAZAR,   4,  "Lnp::DatabaseAlcazar" }
    ,{ ECDriverId::ERESOLVER_DIRVER_HTTP_COUREANQ,  5,  "Lnp::DatabaseCoureAnq" }
}};

#endif /* SERVER_SRC_DRIVERS_DRIVERDEFINES_H_ */
