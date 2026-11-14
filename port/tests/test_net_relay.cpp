// The Free Direct server's addresses and room IDs (net/websocket.h, net/relay.h): what players type
// or paste.
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
// Invite: what a paste on the server screen fills in (server, room; nullptr: nothing found).
static void Paste(const char* text, const char* wantServer, const char* wantRoom)
{
    std::string server, room;
    const bool ok = Invite(text, server, room);
    const bool want = wantServer != nullptr || wantRoom != nullptr;
    if (ok != want || server != (wantServer ? wantServer : "") || room != (wantRoom ? wantRoom : "")) {
        std::printf("FAIL Invite(\"%s\"): %s \"%s\" \"%s\"\n", text, ok ? "found" : "nothing", server.c_str(),
                    room.c_str());
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
    // Pasted: the ID alone (as copied, or as typed), a link of the web page, a sentence.
    Paste("ABCDE-FGHJK-MNPQR-STVWX", nullptr, "ABCDE-FGHJK-MNPQR-STVWX");
    Paste("  abcde fghjk mnpqr stvwx\r\n", nullptr, "ABCDE-FGHJK-MNPQR-STVWX");
    Paste("ABCDEFGHJKMNPQRSTVWX", nullptr, "ABCDE-FGHJK-MNPQR-STVWX");
    Paste("https://openeggbert.github.io/blupi_recon/?server=fds.openeggbert.com&room=ABCDE-FGHJK-MNPQR-STVWX",
          "fds.openeggbert.com", "ABCDE-FGHJK-MNPQR-STVWX");
    Paste("https://example.org/?test-x=1&server=wss%3A%2F%2Fexample.org%3A8443%2Frelay&room=abcde-fghjk-mnpqr-stvwx#top",
          "wss://example.org:8443/relay", "ABCDE-FGHJK-MNPQR-STVWX");
    Paste("https://example.org/?server=example.org", "example.org", nullptr);
    Paste("https://example.org/?server=example.org&room=nonsense", "example.org", nullptr);
    Paste("https://example.org/?chatroom=ABCDE-FGHJK-MNPQR-STVWX&server=example.org", "example.org", nullptr);
    Paste("Room: ABCDE-FGHJK-MNPQR-STVWX (fds.openeggbert.com), come!", nullptr, "ABCDE-FGHJK-MNPQR-STVWX");
    Paste("join my game, room ABCDE FGHJK MNPQR STVWX", nullptr, nullptr);
    Paste("ask the internationalization team", nullptr, nullptr);
    Paste("fds.openeggbert.com", nullptr, nullptr);
    Paste("", nullptr, nullptr);
    std::printf("%s\n", failures ? "FAILED" : "server addresses, room IDs and pastes: ok");
    return failures ? 1 : 0;
}
