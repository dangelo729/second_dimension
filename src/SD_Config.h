// SPDX-License-Identifier: MIT
#ifndef SD_CONFIG_H
#define SD_CONFIG_H

// library-wide configuration: the avr guard, the version and the
// flash-placement macro. every header includes this one.

// avr is refused: the library is written for 32-bit boards and draws every
// pixel with float math. no other board is checked.
#if defined(__AVR__)
#error "SecondDimension needs a 32-bit board with a C++14 compiler. AVR is not supported."
#endif

#define SECONDDIMENSION_VERSION_MAJOR 0
#define SECONDDIMENSION_VERSION_MINOR 3
#define SECONDDIMENSION_VERSION_PATCH 0

// flash placement for generated sprite planes. on teensy it is progmem,
// without which teensy 4's linker copies const data into ram at boot;
// elsewhere it is empty. the blit reads the planes through plain pointers.
#if defined(TEENSYDUINO)
  #include <Arduino.h>
  #define SD_PROGMEM PROGMEM
#else
  #define SD_PROGMEM
#endif

#endif  // SD_CONFIG_H
