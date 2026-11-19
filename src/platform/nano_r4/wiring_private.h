// Empty stand-in for the "wiring_private.h" header, which the Renesas core of the Nano R4 does not have.
// Adafruit_GC9A01A.cpp includes it unconditionally, but only Adafruit_SPITFT's 8-bit parallel mode (not used here:
// the display is on the hardware SPI) calls what it declares (pinPeripheral). Used by the nano_r4 environment only
// (build_flags = -Isrc/platform/nano_r4 in platformio.ini).
#pragma once
