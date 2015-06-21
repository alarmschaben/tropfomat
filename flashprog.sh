#!/bin/bash
/opt/Espressif/esptool/esptool.py -p /dev/ttyUSB0 write_flash -fs 4m 0x02000 firmware/rom0.bin 0x42000 firmware/rom1.bin
#/opt/Espressif/esptool/esptool.py -p /dev/ttyUSB0 write_flash -fs 4m 0x42000 firmware/rom1.bin
