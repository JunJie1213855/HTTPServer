#ifndef SSL_CONFIG_H_
#define SSL_CONFIG_H_

#include <string>

#include "SslTypes.h"

namespace ssl
{

    // Plain data holder. Identical surface to the original muduo-era SslConfig
    // so application code that constructed one keeps compiling unchanged.
    class SslConfig
    {
    public:
        SslConfig() = default;
        ~SslConfig() = default;

        void setCertificateFile(const std::string &f) { certFile_ = f; }
        void setPrivateKeyFile(const std::string &f) { keyFile_ = f; }
        void setCertificateChainFile(const std::string &f) { chainFile_ = f; }
        void setProtocolVersion(SSLVersion v) { version_ = v; }
        void setCipherList(const std::string &c) { cipherList_ = c; }
        void setVerifyClient(bool v) { verifyClient_ = v; }
        void setVerifyDepth(int d) { verifyDepth_ = d; }
        void setSessionTimeout(int s) { sessionTimeout_ = s; }
        void setSessionCacheSize(long s) { sessionCacheSize_ = s; }

        const std::string &getCertificateFile() const { return certFile_; }
        const std::string &getPrivateKeyFile() const { return keyFile_; }
        const std::string &getCertificateChainFile() const { return chainFile_; }
        SSLVersion getProtocolVersion() const { return version_; }
        const std::string &getCipherList() const { return cipherList_; }
        bool getVerifyClient() const { return verifyClient_; }
        int getVerifyDepth() const { return verifyDepth_; }
        int getSessionTimeout() const { return sessionTimeout_; }
        long getSessionCacheSize() const { return sessionCacheSize_; }

    private:
        std::string certFile_;
        std::string keyFile_;
        std::string chainFile_;
        SSLVersion version_{SSLVersion::TLS_1_2};
        std::string cipherList_;
        bool verifyClient_{false};
        int verifyDepth_{4};
        int sessionTimeout_{3600};
        long sessionCacheSize_{20480};
    };

} // namespace ssl

#endif
