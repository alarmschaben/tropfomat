# Tropfomat

This repository holds the code for my balcony drip control unit. A more detailed system description is to follow.

Distributed under the MIT license unless otherwise specified.

## Notes

Adapt `Makefile` to point to Espressif IoT SDK directory.

Create file `wifi.h` with following content:

    #define SSID "Your SSID"
    #define PASS "Your Password"

When issuing `make`, two files are created, `rom0.bin` and `rom1.bin`. These are the two halves of your initial ROM that have be be flashed to the ESP's flash along with the rboot bootloader (not included).

See rboot documentation for details.

## Acknowledgments

Based on the sample project for the fine [rboot bootloader](https://github.com/raburton/esp8266) by @raburton

Contains DHT driver source code by Jeroen Domburg <jeroen@spritesmods.com>

Comtains [MQTT library](https://github.com/tuanpmt/esp_mqtt) by @tuanpmt.
