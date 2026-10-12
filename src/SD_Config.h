// SPDX-License-Identifier: MIT
#ifndef SD_CONFIG_H
#define SD_CONFIG_H

// library-wide configuration: the platform guard, the version and the
// flash-placement macro. every other header includes this first, so the
// guard fires whichever header a sketch reaches for.

// the floor is a 32-bit mcu with hardware float: the blit carries every
// target pixel back through a float matrix. avr has neither.
#if defined(__AVR__)
#error "SecondDimension requires a 32-bit MCU with hardware float (Teensy 4.x, ESP32, STM32F4+, RP2350). AVR is not supported."
#endif

#define SECONDDIMENSION_VERSION_MAJOR 0
#define SECONDDIMENSION_VERSION_MINOR 1
#define SECONDDIMENSION_VERSION_PATCH 0

// flash placement for generated sprite planes. teensy needs the progmem
// section attribute, or its linker copies const data into ram at boot;
// esp32 / stm32 / rp2350 map const rodata into flash natively. the blit
// reads the planes through plain pointers either way (no pgm_read_*).
#if defined(TEENSYDUINO)
  #include <Arduino.h>
  #define SD_PROGMEM PROGMEM
#else
  #define SD_PROGMEM
#endif

#endif  // SD_CONFIG_H
