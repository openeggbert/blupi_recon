// The port's server address screen (common/ui/address_screen.cpp): the Internet entry of
// the multiplayer provider list asks for a host, as DirectPlay's TCP/IP provider did with
// a dialog of its own. Shared event/presentation API, like options_events.h.
#ifndef BLUPI_UI_ADDRESS_EVENTS_H
#define BLUPI_UI_ADDRESS_EVENTS_H
#include <cstdint>
#include <string>
void     PortAddressRequest();                // the Internet provider was chosen
void     PortServerRequest();                 // the Free Direct server provider was chosen
void     PortAddressPhase(unsigned int phase);  // every frame: opens it on the session screen
bool     PortAddressOpen();
// Input for it; true when the event is not for the game.
bool     PortAddressMouse(unsigned int message, uintptr_t wParam, intptr_t lParam);
bool     PortAddressKey(unsigned int message, uintptr_t wParam);
bool     PortAddressCommand(unsigned int message);   // its buttons' posted messages
void     PortAddressPaste(const std::string& text);  // the clipboard's text it asked for
void     PortAddressDraw();                   // into the back buffer, after the frame

#endif
