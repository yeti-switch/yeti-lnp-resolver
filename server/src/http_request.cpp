#include "http_request.h"

bool http_request::s_initialize = false;

http_request::http_request()
    : m_ctx(nullptr), m_hdrs_list(NULL)
{
    m_errBuffer[0] = '\0';

    if (false == s_initialize)
    {
        // initialize global environemnt
        if (curl_global_init(CURL_GLOBAL_ALL))
		    throw error("global environment initializing error");

        s_initialize = true;
    }

    // create request context
    m_ctx = curl_easy_init();
    if (!m_ctx)
        throw error("context creation error");
}

http_request::~http_request()
{
    if (NULL != m_hdrs_list)
    {
        curl_slist_free_all(m_hdrs_list);
    }

    if (nullptr != m_ctx)
    {
        curl_easy_cleanup(m_ctx);
    }
}

void http_request::setSSLVerification(bool onoff)
{
    const long verify = onoff ? 2L : 0L;

    if ((curl_easy_setopt(m_ctx, CURLOPT_SSL_VERIFYPEER, verify) != CURLE_OK) ||
        (curl_easy_setopt(m_ctx, CURLOPT_SSL_VERIFYHOST, verify) != CURLE_OK))
        throw error("SSL verification option error");
}

void http_request::setAuthType(auth_t type)
{
    if (CURLE_OK != curl_easy_setopt(m_ctx, CURLOPT_HTTPAUTH, type))
        throw error("authorization type handling error");
}

void http_request::setAuthData(const char * login, const char * pass)
{
    if ((curl_easy_setopt(m_ctx,  CURLOPT_USERNAME, login) != CURLE_OK) ||
        (curl_easy_setopt(m_ctx,  CURLOPT_PASSWORD, pass)  != CURLE_OK))
        throw error("authorization data error");
}

void http_request::setAuthData(const string & login, const string & pass)
{
    return setAuthData(login.c_str(), pass.c_str());
}

void http_request::setTimeout(const long timeout)
{
    if ((curl_easy_setopt(m_ctx, CURLOPT_TIMEOUT_MS, timeout) != CURLE_OK) ||
        (curl_easy_setopt(m_ctx, CURLOPT_NOSIGNAL, 1L)        != CURLE_OK))
        throw error("set up timeout value error");
}

void http_request::setHeader(const char * header)
{
    hdr_t * hdr_list = curl_slist_append(m_hdrs_list, header);
    if (!hdr_list)
        throw error("error appending header to request");

    m_hdrs_list = hdr_list;
}

void http_request::setHeader(const string & header)
{
    return setHeader(header.c_str());
}

void http_request::setWriteCallback(wcb_fn fn, void * userdata)
{
    if ((curl_easy_setopt(m_ctx, CURLOPT_WRITEFUNCTION, fn)   != CURLE_OK) ||
        (curl_easy_setopt(m_ctx, CURLOPT_WRITEDATA, userdata) != CURLE_OK))
        throw error("initializing write callback data error");
}

const char * http_request::getErrorStr() const
{
    return m_errBuffer;
}

http_request::req_code_t http_request::perform(const char * url)
{
    if (curl_easy_setopt(m_ctx, CURLOPT_URL, url) != CURLE_OK)
        throw error("URL processing error");

    if (m_hdrs_list)
    {
        if (curl_easy_setopt(m_ctx, CURLOPT_HTTPHEADER, m_hdrs_list) != CURLE_OK)
            throw error("headers processing error");
    }

    if(curl_easy_setopt(m_ctx, CURLOPT_ERRORBUFFER, m_errBuffer) != CURLE_OK)
        throw error("error buffer initializing error");

    CURLcode rc = curl_easy_perform(m_ctx);

    return (CURLE_OK == rc) ? REQ_CODE::OK : REQ_CODE::FAIL;
}

http_request::req_code_t http_request::perform(const string & url)
{
    return perform(url.c_str());
}



