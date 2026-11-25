.SUFFIXES:
ifeq ($(strip $(DEVKITPRO)),)
$(error Please set DEVKITPRO)
endif
TOPDIR ?= $(CURDIR)
include $(DEVKITPRO)/libnx/switch_rules
TARGET := NavidromeNX
BUILD := build
SOURCES := source ../vendor/sys-tune/ipc
INCLUDES := ../src source ../vendor/sys-tune/ipc ../vendor/aurore/library/include/aurore/extern
include $(TOPDIR)/../vendor/libultrahand/ultrahand.mk
APP_TITLE := Navidrome NX
APP_AUTHOR := Navidrome NX contributors
APP_VERSION := 1.1.0
ARCH := -march=armv8-a+crc+crypto -mtune=cortex-a57 -mtp=soft -fPIE
CFLAGS := -g -Os -Wall -ffunction-sections -fdata-sections $(ARCH) $(INCLUDE) -D__SWITCH__ -DAPP_VERSION=\"$(APP_VERSION)\" -DUI_OVERRIDE_PATH=\"/config/navidrome_nx_overlay/\"
CXXFLAGS := $(CFLAGS) -std=gnu++23 -fno-rtti
ASFLAGS := $(ARCH)
LDFLAGS := -specs=$(DEVKITPRO)/libnx/switch.specs $(ARCH) -Wl,--gc-sections,-Map,$(notdir $*.map)
LIBS := -ljansson -lcurl -lminizip -lz -lmbedtls -lmbedx509 -lmbedcrypto -lnx
LIBDIRS := $(PORTLIBS) $(LIBNX)
ifneq ($(BUILD),$(notdir $(CURDIR)))
export OUTPUT := $(CURDIR)/$(TARGET)
export TOPDIR := $(CURDIR)
export VPATH := $(foreach dir,$(SOURCES),$(CURDIR)/$(dir))
export DEPSDIR := $(CURDIR)/$(BUILD)
export LD := $(CXX)
export OFILES := $(foreach dir,$(SOURCES),$(notdir $(patsubst %.cpp,%.o,$(wildcard $(dir)/*.cpp)) $(patsubst %.c,%.o,$(wildcard $(dir)/*.c))))
export INCLUDE := $(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) $(foreach dir,$(LIBDIRS),-I$(dir)/include) -I$(CURDIR)/$(BUILD)
export LIBPATHS := $(foreach dir,$(LIBDIRS),-L$(dir)/lib)
.PHONY: all clean $(BUILD)
all: $(BUILD)
$(BUILD):
	@mkdir -p $@
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile
clean:
	rm -rf $(BUILD) $(TARGET).ovl $(TARGET).elf $(TARGET).nacp
else
all: $(OUTPUT).ovl
$(OUTPUT).ovl: $(OUTPUT).elf $(OUTPUT).nacp
	elf2nro $< $@ --nacp=$(OUTPUT).nacp
$(OUTPUT).elf: $(OFILES)
-include $(OFILES:.o=.d)
endif
