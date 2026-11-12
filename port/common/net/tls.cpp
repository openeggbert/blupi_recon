// TLS for wss:// (tls.h): an Mbed TLS client over the WebSocket client's non-blocking socket.
#include "net/tls.h"

#if !(defined(BLUPI_TLS) && BLUPI_TLS)

namespace blupi::net {
struct Tls::Impl {};
bool Tls::Built() { return false; }
Tls::Tls() = default;
Tls::~Tls() = default;
bool Tls::Start(const std::string&, Io&, std::string& error)
{
    error = "this build has no wss:// (built without BLUPI_TLS)";
    return false;
}
int Tls::Handshake(std::string&) { return -1; }
int Tls::Write(const uint8_t*, size_t, std::string&) { return -1; }
int Tls::Read(uint8_t*, size_t, std::string&) { return kBroken; }
void Tls::Stop() {}
} // namespace blupi::net

#else

#include <cstdio>
#include <cstdlib>
#include <mbedtls/error.h>
#include <mbedtls/net_sockets.h>
extern "C" {   // psa_util.h, unlike the other headers, has no C++ guard
#include <mbedtls/psa_util.h>
}
#include <mbedtls/ssl.h>
#include <mbedtls/x509_crt.h>
#include <psa/crypto.h>
#ifdef _WIN32
#include <windows.h>
#include <wincrypt.h>
#endif

namespace blupi::net {
namespace {
// What all connections share, set up by the first wss:// connection and kept for the program's life:
// the client configuration and the trusted certificates.
struct Shared {
    mbedtls_ssl_config conf;
    mbedtls_x509_crt ca;
    bool systemCheck = false;   // Windows: the system checks the chain and the name after the handshake
    std::string error;          // why there is no TLS
};

std::string Text(int code)
{
    char text[160];
    mbedtls_strerror(code, text, sizeof(text));
    return text;
}

// True if the certificates are in (a file or a directory of files; ones it cannot read are skipped).
bool Load(mbedtls_x509_crt& ca, const char* path, bool directory)
{
    const int ret = directory ? mbedtls_x509_crt_parse_path(&ca, path) : mbedtls_x509_crt_parse_file(&ca, path);
    return ret >= 0 && ca.version != 0;
}

Shared* Setup()
{
    auto* s = new Shared;
    mbedtls_ssl_config_init(&s->conf);
    mbedtls_x509_crt_init(&s->ca);
    if (psa_crypto_init() != PSA_SUCCESS) {
        s->error = "no secure random numbers on this system";
        return s;
    }
    const char* file = std::getenv("SSL_CERT_FILE");
    const char* dir = std::getenv("SSL_CERT_DIR");
    bool loaded = false;
    if ((file && *file) || (dir && *dir)) {
        if (file && *file) loaded = Load(s->ca, file, false) || loaded;
        if (dir && *dir) loaded = Load(s->ca, dir, true) || loaded;
        if (!loaded) s->error = "no certificates in SSL_CERT_FILE or SSL_CERT_DIR";
    } else {
#if defined(_WIN32)
        s->systemCheck = true;
#elif defined(__ANDROID__)
        // Android 14 and later update them in the Conscrypt module; older ones keep them in /system.
        for (const char* d : {"/apex/com.android.conscrypt/cacerts", "/system/etc/security/cacerts"})
            if (!loaded) loaded = Load(s->ca, d, true);
#else
        // The CA bundle where the distributions keep it (Debian/Ubuntu/Arch, Fedora/RHEL, openSUSE,
        // RHEL 7+, Alpine), else one certificate per file.
        for (const char* f : {"/etc/ssl/certs/ca-certificates.crt", "/etc/pki/tls/certs/ca-bundle.crt",
                              "/etc/ssl/ca-bundle.pem", "/etc/pki/ca-trust/extracted/pem/tls-ca-bundle.pem",
                              "/etc/ssl/cert.pem"})
            if (!loaded) loaded = Load(s->ca, f, false);
        for (const char* d : {"/etc/ssl/certs", "/etc/pki/tls/certs"})
            if (!loaded) loaded = Load(s->ca, d, true);
#endif
        if (!loaded && !s->systemCheck) s->error = "no trusted certificates on this system (set SSL_CERT_FILE)";
    }
    if (!s->error.empty()) return s;
    const int ret = mbedtls_ssl_config_defaults(&s->conf, MBEDTLS_SSL_IS_CLIENT, MBEDTLS_SSL_TRANSPORT_STREAM,
                                                MBEDTLS_SSL_PRESET_DEFAULT);
    if (ret != 0) {
        s->error = "no TLS (" + Text(ret) + ")";
        return s;
    }
    mbedtls_ssl_conf_rng(&s->conf, mbedtls_psa_get_random, MBEDTLS_PSA_RANDOM_STATE);
    if (s->systemCheck) {
        // Mbed TLS checks the handshake's signatures; the chain and the name are the system's to judge.
        mbedtls_ssl_conf_authmode(&s->conf, MBEDTLS_SSL_VERIFY_OPTIONAL);
    } else {
        mbedtls_ssl_conf_authmode(&s->conf, MBEDTLS_SSL_VERIFY_REQUIRED);
        mbedtls_ssl_conf_ca_chain(&s->conf, &s->ca, nullptr);
    }
    return s;
}

Shared& Common()
{
    static Shared* shared = Setup();
    return *shared;
}

std::string CertificateError(uint32_t flags)
{
    if (flags & MBEDTLS_X509_BADCERT_NOT_TRUSTED) return "the server's certificate is not trusted";
    if (flags & MBEDTLS_X509_BADCERT_CN_MISMATCH) return "the server's certificate is for another name";
    if (flags & MBEDTLS_X509_BADCERT_EXPIRED) return "the server's certificate has expired";
    if (flags & MBEDTLS_X509_BADCERT_FUTURE) return "the server's certificate is not valid yet";
    return "the server's certificate is not valid";
}

std::string HandshakeError(int code, const mbedtls_ssl_context& ssl)
{
    switch (code) {
    case MBEDTLS_ERR_X509_CERT_VERIFY_FAILED:
    case MBEDTLS_ERR_SSL_BAD_CERTIFICATE:
        return CertificateError(mbedtls_ssl_get_verify_result(&ssl));
    case MBEDTLS_ERR_SSL_CONN_EOF:
        return "the server closed the secure connection";
    case MBEDTLS_ERR_NET_RECV_FAILED:
    case MBEDTLS_ERR_NET_SEND_FAILED:
        return "the connection to the server broke";
    case MBEDTLS_ERR_SSL_INVALID_RECORD:
    case MBEDTLS_ERR_SSL_BAD_PROTOCOL_VERSION:
        return "the server does not speak wss:// here (try ws://)";
    default:
        return "the secure connection failed (" + Text(code) + ")";
    }
}

#ifdef _WIN32
std::string WindowsError(DWORD code)
{
    if (code == DWORD(CERT_E_CN_NO_MATCH)) return "the server's certificate is for another name";
    if (code == DWORD(CERT_E_EXPIRED)) return "the server's certificate has expired or is not valid yet";
    if (code == DWORD(CERT_E_UNTRUSTEDROOT) || code == DWORD(CERT_E_CHAINING) ||
        code == DWORD(CERT_E_UNTRUSTEDTESTROOT) || code == DWORD(TRUST_E_CERT_SIGNATURE))
        return "the server's certificate is not trusted";
    if (code == DWORD(CRYPT_E_REVOKED)) return "the server's certificate has been revoked";
    if (code == DWORD(CERT_E_WRONG_USAGE)) return "the server's certificate is not for servers";
    char text[96];
    std::snprintf(text, sizeof(text), "the server's certificate is not valid (Windows error 0x%08lX)",
                  (unsigned long)code);
    return text;
}

// The system's check of the certificates the server sent (leaf first) for host: "" if they pass.
std::string SystemCheck(const mbedtls_x509_crt* chain, const std::string& host)
{
    if (!chain) return "the server sent no certificate";
    HCERTSTORE store = CertOpenStore(CERT_STORE_PROV_MEMORY, 0, 0, CERT_STORE_CREATE_NEW_FLAG, nullptr);
    if (!store) return "cannot check the server's certificate";
    PCCERT_CONTEXT leaf = nullptr;
    for (const mbedtls_x509_crt* c = chain; c; c = c->next)
        CertAddEncodedCertificateToStore(store, X509_ASN_ENCODING, c->raw.p, DWORD(c->raw.len),
                                         CERT_STORE_ADD_ALWAYS, c == chain ? &leaf : nullptr);
    std::string why = "cannot check the server's certificate";
    CERT_CHAIN_PARA para{};
    para.cbSize = sizeof(para);
    LPSTR usage = const_cast<LPSTR>(szOID_PKIX_KP_SERVER_AUTH);
    para.RequestedUsage.dwType = USAGE_MATCH_TYPE_AND;
    para.RequestedUsage.Usage.cUsageIdentifier = 1;
    para.RequestedUsage.Usage.rgpszUsageIdentifier = &usage;
    PCCERT_CHAIN_CONTEXT context = nullptr;
    if (leaf && CertGetCertificateChain(nullptr, leaf, nullptr, store, &para, 0, nullptr, &context)) {
        // The chain's own trust status first, as curl's Schannel check does: under Wine the SSL
        // policy alone passed a certificate of an unknown CA whose server sent no chain (net_tls,
        // "the system's trust"). No revocation check is asked for, so its "unknown" bits are no
        // error; nor is the obsolete time nesting.
        const DWORD trust = context->TrustStatus.dwErrorStatus &
                            ~DWORD(CERT_TRUST_IS_NOT_TIME_NESTED | CERT_TRUST_REVOCATION_STATUS_UNKNOWN |
                                   CERT_TRUST_IS_OFFLINE_REVOCATION);
        if (trust != CERT_TRUST_NO_ERROR) {
            if (trust & CERT_TRUST_IS_REVOKED) why = "the server's certificate has been revoked";
            else if (trust & (CERT_TRUST_IS_PARTIAL_CHAIN | CERT_TRUST_IS_UNTRUSTED_ROOT |
                              CERT_TRUST_IS_NOT_SIGNATURE_VALID | CERT_TRUST_IS_EXPLICIT_DISTRUST |
                              CERT_TRUST_IS_CYCLIC))
                why = "the server's certificate is not trusted";
            else if (trust & CERT_TRUST_IS_NOT_TIME_VALID) why = "the server's certificate has expired or is not valid yet";
            else if (trust & CERT_TRUST_IS_NOT_VALID_FOR_USAGE) why = "the server's certificate is not for servers";
            else {
                char text[96];
                std::snprintf(text, sizeof(text), "the server's certificate is not valid (Windows trust 0x%08lX)",
                              (unsigned long)trust);
                why = text;
            }
            CertFreeCertificateChain(context);
            CertFreeCertificateContext(leaf);
            CertCloseStore(store, 0);
            return why;
        }
        std::wstring name(host.begin(), host.end());   // a server name is ASCII (ServerUrl)
        SSL_EXTRA_CERT_CHAIN_POLICY_PARA extra{};
        extra.cbSize = sizeof(extra);
        extra.dwAuthType = AUTHTYPE_SERVER;
        extra.pwszServerName = &name[0];
        CERT_CHAIN_POLICY_PARA policy{};
        policy.cbSize = sizeof(policy);
        policy.pvExtraPolicyPara = &extra;
        CERT_CHAIN_POLICY_STATUS status{};
        status.cbSize = sizeof(status);
        if (CertVerifyCertificateChainPolicy(CERT_CHAIN_POLICY_SSL, context, &policy, &status))
            why = status.dwError == 0 ? "" : WindowsError(status.dwError);
        CertFreeCertificateChain(context);
    }
    if (leaf) CertFreeCertificateContext(leaf);
    CertCloseStore(store, 0);
    return why;
}
#endif

int SendCallback(void* io, const unsigned char* data, size_t size)
{
    const int n = static_cast<Tls::Io*>(io)->Send(data, size);
    return n > 0 ? n : n == 0 ? MBEDTLS_ERR_SSL_WANT_WRITE : MBEDTLS_ERR_NET_SEND_FAILED;
}

int ReceiveCallback(void* io, unsigned char* data, size_t size)
{
    const int n = static_cast<Tls::Io*>(io)->Receive(data, size);
    if (n > 0) return n;
    if (n == 0) return MBEDTLS_ERR_SSL_WANT_READ;
    return n == Tls::kClosed ? 0 : MBEDTLS_ERR_NET_RECV_FAILED;   // 0: the end of the connection
}

bool Again(int code)
{
    return code == MBEDTLS_ERR_SSL_WANT_READ || code == MBEDTLS_ERR_SSL_WANT_WRITE ||
           code == MBEDTLS_ERR_SSL_RECEIVED_NEW_SESSION_TICKET;
}
} // namespace

struct Tls::Impl {
    mbedtls_ssl_context ssl;
    bool started = false, open = false;
    size_t pending = 0;   // the length of a write that would have blocked
    std::string host;
    Impl() { mbedtls_ssl_init(&ssl); }
    ~Impl() { mbedtls_ssl_free(&ssl); }
    void Reset()
    {
        mbedtls_ssl_free(&ssl);
        mbedtls_ssl_init(&ssl);
        started = open = false;
        pending = 0;
        host.clear();
    }
};

bool Tls::Built() { return true; }
Tls::Tls() : p(new Impl) {}
Tls::~Tls() { Stop(); }

bool Tls::Start(const std::string& host, Io& io, std::string& error)
{
    Stop();
    Shared& shared = Common();
    if (!shared.error.empty()) {
        error = shared.error;
        return false;
    }
    int ret = mbedtls_ssl_setup(&p->ssl, &shared.conf);
    if (ret == 0) ret = mbedtls_ssl_set_hostname(&p->ssl, host.c_str());
    if (ret != 0) {
        error = "the secure connection failed (" + Text(ret) + ")";
        p->Reset();
        return false;
    }
    mbedtls_ssl_set_bio(&p->ssl, &io, SendCallback, ReceiveCallback, nullptr);
    p->host = host;
    p->started = true;
    return true;
}

int Tls::Handshake(std::string& error)
{
    if (p->open) return 1;
    if (!p->started) {
        error = "no secure connection";
        return -1;
    }
    const int ret = mbedtls_ssl_handshake(&p->ssl);
    if (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE) return 0;
    if (ret != 0) {
        error = HandshakeError(ret, p->ssl);
        return -1;
    }
#ifdef _WIN32
    if (Common().systemCheck) {
        const std::string why = SystemCheck(mbedtls_ssl_get_peer_cert(&p->ssl), p->host);
        if (!why.empty()) {
            error = why;
            return -1;
        }
    }
#endif
    p->open = true;
    return 1;
}

int Tls::Write(const uint8_t* data, size_t size, std::string& error)
{
    if (!p->open) {
        error = "no secure connection";
        return -1;
    }
    size_t length = p->pending;
    if (length == 0) {
        const int most = mbedtls_ssl_get_max_out_record_payload(&p->ssl);
        length = most > 0 && size > size_t(most) ? size_t(most) : size;
    }
    const int ret = mbedtls_ssl_write(&p->ssl, data, length);
    if (Again(ret)) {
        p->pending = length;
        return 0;
    }
    p->pending = 0;
    if (ret < 0) {
        error = ret == MBEDTLS_ERR_NET_SEND_FAILED ? "the connection to the server broke"
                                                   : "the secure connection broke (" + Text(ret) + ")";
        return -1;
    }
    return ret;
}

int Tls::Read(uint8_t* data, size_t size, std::string& error)
{
    if (!p->open) {
        error = "no secure connection";
        return kBroken;
    }
    for (int round = 0; round < 8; ++round) {
        const int ret = mbedtls_ssl_read(&p->ssl, data, size);
        if (ret > 0) return ret;
        if (ret == MBEDTLS_ERR_SSL_RECEIVED_NEW_SESSION_TICKET) continue;
        if (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE) return 0;
        if (ret == 0 || ret == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY || ret == MBEDTLS_ERR_SSL_CONN_EOF) return kClosed;
        error = ret == MBEDTLS_ERR_NET_RECV_FAILED ? "the connection to the server broke"
                                                   : "the secure connection broke (" + Text(ret) + ")";
        return kBroken;
    }
    return 0;
}

void Tls::Stop()
{
    if (p->open) mbedtls_ssl_close_notify(&p->ssl);   // best effort: the socket closes next
    p->Reset();
}
} // namespace blupi::net

#endif
