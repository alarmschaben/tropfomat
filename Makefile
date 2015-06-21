BUILD_DIR = build
FIRMW_DIR = firmware
MQTT_DIR = esp_mqtt/mqtt

LIBS    = c gcc hal phy net80211 lwip wpa main pp ssl
CFLAGS  = -Os -g -O2 -Wpointer-arith -Wundef -Werror -Wl,-EL -fno-inline-functions -nostdlib -mlongcalls -mtext-section-literals  -D__ets__ -DICACHE_FLASH
LDFLAGS = -nostdlib -Wl,--no-check-sections -u call_user_start -Wl,-static

SDK_BASE   = /opt/Espressif/esp_iot_sdk_v1.1.1
SDK_LIBDIR = lib
SDK_INCDIR = include

FW_TOOL       = /opt/Espressif/esptool2/esptool2
FW_SECTS      = .text .data .rodata
FW_USER_ARGS  = -quiet -bin -boot2

CC		:= /opt/Espressif/xtensa-lx106-elf/bin/xtensa-lx106-elf-gcc
LD		:= /opt/Espressif/xtensa-lx106-elf/bin/xtensa-lx106-elf-gcc

SDK_LIBDIR	:= $(addprefix $(SDK_BASE)/,$(SDK_LIBDIR))
SDK_INCDIR	:= $(addprefix -I$(SDK_BASE)/,$(SDK_INCDIR))

MQTT_SRC	:= $(wildcard $(MQTT_DIR)/*.c)
MQTT_OBJ	:= $(patsubst %.c,$(BUILD_DIR)/%.o,$(MQTT_SRC))

MQTT_CFILES	:= $(wildcard $(MQTT_DIR)/*.c)
MQTT_OFILES	:= $(patsubst $(MQTT_DIR)/%.c,$(BUILD_DIR)/%.o,$(MQTT_CFILES))

SRC		:= $(wildcard *.c)
OBJ		:= $(patsubst %.c,$(BUILD_DIR)/%.o,$(SRC))
LIBS	:= $(addprefix -l,$(LIBS))

.SECONDARY:
.PHONY: all clean

C_FILES = $(wildcard *.c)
O_FILES = $(patsubst %.c,$(BUILD_DIR)/%.o,$(C_FILES))

all: $(BUILD_DIR) $(FIRMW_DIR) $(FIRMW_DIR)/rom0.bin $(FIRMW_DIR)/rom1.bin

$(BUILD_DIR)/%.o: %.c %.h
	@echo "CC $<"
	@$(CC) -I. $(SDK_INCDIR) $(CFLAGS) -o $@ -c $<


mqtt: $(BUILD_DIR) $(MQTT_OFILES)

$(BUILD_DIR)/%.o: $(MQTT_DIR)/%.c
	@echo "CC $<"
	@$(CC) -I. -I$(MQTT_DIR)/include $(SDK_INCDIR) $(CFLAGS) -o $@ -c $<

$(BUILD_DIR)/%.elf: $(O_FILES) $(MQTT_OFILES)
	@echo "LD $(notdir $@)"
	@$(LD) -L$(SDK_LIBDIR) -T$(notdir $(basename $@)).ld $(LDFLAGS) -Wl,--start-group $(LIBS) $^ -Wl,--end-group -o $@

$(FIRMW_DIR)/%.bin: $(BUILD_DIR)/%.elf
	@echo "FW $(notdir $@)"
	@$(FW_TOOL) $(FW_USER_ARGS) $^ $@ $(FW_SECTS)

$(BUILD_DIR):
	@mkdir -p $@

$(FIRMW_DIR):
	@mkdir -p $@

clean:
	@rm -rf $(BUILD_DIR)
	@rm -rf $(FIRMW_DIR)
