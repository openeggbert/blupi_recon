// The Free Direct server's addresses and room IDs (net/websocket.h, net/relay.h): what players type.
#include "net/relay.h"
#include "net/websocket.h"
#include <cstdio>
#include <string>
using namespace blupi::net;
static int failures = 0;
static void Url(const char* typed, bool secure, const char* want)
{
    std::string got;
    const bool ok = ServerUrl(typed, secure, got);
    if ((want == nullptr) != !ok || (ok && got != want)) {
        std::printf("FAIL ServerUrl(\"%s\", %s): %s, want %s\n", typed, secure ? "https page" : "desktop",
                    ok ? got.c_str() : "rejected", want ? want : "rejected");
        ++failures;
    }
}
static void Room(const char* typed, const char* want)
{
    std::string got;
    const bool ok = RoomId(typed, got);
    if ((want == nullptr) != !ok || (ok && got != want)) {
        std::printf("FAIL RoomId(\"%s\"): %s\n", typed, ok ? got.c_str() : "rejected");
        ++failures;
    }
}
int main()
{
    // A plain name: the server's own port from the desktop, wss:// on 443 from an https:// page.
    Url("fds.openeggbert.com", false, "ws://fds.openeggbert.com:51325/");
    Url("fds.openeggbert.com", true, "wss://fds.openeggbert.com:443/");
    Url("  FDS.openeggbert.com ", true, "wss://fds.openeggbert.com:443/");
    Url("fds.openeggbert.com:51325", true, "wss://fds.openeggbert.com:51325/");
    Url("127.0.0.1:4000", false, "ws://127.0.0.1:4000/");
    // A URL keeps its scheme; without a port, the scheme's.
    Url("ws://example.org", true, "ws://example.org:80/");
    Url("wss://example.org", false, "wss://example.org:443/");
    Url("wss://example.org:8443/relay", false, "wss://example.org:8443/relay");
    // Not addresses.
    Url("", false, nullptr);
    Url("http://example.org", false, nullptr);
    Url("example.org:0", false, nullptr);
    Url("example.org:70000", false, nullptr);
    Url("exa mple.org", false, nullptr);
    Url("example.org/a b", false, nullptr);
    // Room IDs as the server reads them.
    Room("ABCDE-FGHJK-MNPQR-STVWX", "ABCDEFGHJKMNPQRSTVWX");
    Room("abcde fghjk mnpqr stvwx", "ABCDEFGHJKMNPQRSTVWX");
    Room("O0IL1-00000-00000-00000", "00111000000000000000");
    Room("ABCDE-FGHJK-MNPQR-STVW", nullptr);
    Room("ABCDE-FGHJK-MNPQR-STVWU", nullptr);
    std::printf("%s\n", failures ? "FAILED" : "server addresses and room IDs: ok");
    return failures ? 1 : 0;
}
