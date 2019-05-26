#ifndef SERVER_SRC_LIBS_FMTERROR_H
#define SERVER_SRC_LIBS_FMTERROR_H

#include <cstdio>
using std::snprintf;

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

  mBufLen = snprintf(nullptr, 0, fmt, args ...);
  mBufLen += 1; // Extra space for '\0'

  mBuf.reset(new char [mBufLen]);
  snprintf(mBuf.get(), mBufLen, fmt, args ...);
}

/**
 * @brief Getter for formatted string value
 *
 * @return formatted string
 */
inline string fmterror::get() const
{
  char * bufStart = mBuf.get();
  char * bufEnd   = mBuf.get() + mBufLen;
         bufEnd   -= 1; // We don't want the '\0' inside

  return std::string(bufStart, bufEnd);
}

#endif /* SERVER_SRC_LIBS_FMTERROR_H */

