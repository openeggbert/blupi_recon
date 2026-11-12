// A Free Direct server's welcome over ws:// or wss:// (net/relay.h, net/tls.h): the probe of the TLS
// tests (net_tls.py), and a check of a real server (net_tls_online, BLUPI_TEST_ONLINE_SERVER).
//   test_net_tls <address> ok             the server welcomes the probe
//   test_net_tls <address> fail <text>    connecting fails, and the error says <text>
// Addresses are read as the desktop reads typed ones (ServerUrl). The certificates trusted are the
// program's usual ones: SSL_CERT_FILE / SSL_CERT_DIR if set, else the system's.
#include "net/relay.h"
#include "net/websocket.h"
#include <cstdio>
#include <cstring>
#include <string>
using namespace blupi::net;

int main(int argc, char** argv)
{
    const bool wantWelcome = argc == 3 && std::strcmp(argv[2], "ok") == 0;
    if (!wantWelcome && !(argc == 4 && std::strcmp(argv[2], "fail") == 0)) {
        std::fprintf(stderr, "usage: test_net_tls <address> ok | test_net_tls <address> fail <text>\n");
        return 2;
    }
    std::string url;
    if (!ServerUrl(argv[1], false, url)) {
        std::printf("FAIL: %s is not a server address\n", argv[1]);
        return 1;
    }
    Relay relay;
    const bool welcomed = relay.Connect(url, "", "blupi-sdl3/tls-check");
    const std::string error = relay.Error();
    relay.Disconnect();
    if (welcomed) std::printf("%s: welcome\n", url.c_str());
    else std::printf("%s: %s\n", url.c_str(), error.c_str());
    if (welcomed != wantWelcome || (!welcomed && error.find(argv[3]) == std::string::npos)) {
        std::printf("FAIL: wanted %s%s%s\n", wantWelcome ? "a welcome" : "an error saying \"",
                    wantWelcome ? "" : argv[3], wantWelcome ? "" : "\"");
        return 1;
    }
    return 0;
}
