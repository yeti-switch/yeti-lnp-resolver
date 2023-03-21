#ifndef SERVER_SRC_LIBS_FMTERROR_H
#define SERVER_SRC_LIBS_FMTERROR_H

#include <stdio.h>

#include <string>
using std::string;

#include <memory>
using std::unique_ptr;

/**
 * @brief The printf like format error class
 */
class fmterror
{
  private:
    unique_ptr<char []> mBuf;
    size_t mBufLen;

  public:
    template <typename ... Args>
    explicit fmterror(const char * fmt, Args ... args) noexcept;

    fmterror(const fmterror & e)            = delete;
    fmterror & operator=(const fmterror & e) = delete;

    string get() const;
};

/**
 * @brief Format error class constructor
 *
 * @param[in] fmt   The string format
 * @param[in] args  Variable arguments for formatting
 */
template <typename ... Args>
fmterror::fmterror(const char * fmt, Args ... args) noexcept
{
  if (!fmt)
  {
    fmt = "unknown error";
  }
  
  char* tmp_buf;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-security"
  mBufLen = asprintf(&tmp_buf, fmt, args ...);
#pragma GCC diagnostic pop

  mBuf.reset(tmp_buf);
}

/**
 * @brief Getter for formatted string value
 *
 * @return formatted string
 */
inline string fmterror::get() const
{
  return string(mBuf.get(), mBufLen);
}

#endif /* SERVER_SRC_LIBS_FMTERROR_H */
