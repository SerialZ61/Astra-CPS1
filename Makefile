.DEFAULT_GOAL := all
.DELETE_ON_ERROR:
ifndef DEVKITARM
$(error Use the devkitPro shell with devkitARM and libctru installed)
endif
include build/core.mk

CC := $(DEVKITARM)/bin/arm-none-eabi-gcc
CXX := $(DEVKITARM)/bin/arm-none-eabi-g++
SDK := $(DEVKITPRO)/libctru
PORTS := $(DEVKITPRO)/portlibs/3ds
ARCH := -march=armv6k -mtune=mpcore -mfpu=vfp -mfloat-abi=hard -marm -mtp=soft
COMMON := $(ARCH) -O3 -g1 -mword-relocations -fomit-frame-pointer -ffunction-sections -fdata-sections -D__3DS__
INCLUDES := -Iengine -I$(SDK)/include -I$(PORTS)/include $(CORE_INCLUDES)
OBJ := out/obj
APP := out/AstraCPS1
include build/display.mk
include build/compat.mk
APP_C := app/main.c app/display.c app/sound.c app/stretch.c app/pacing.c app/library.c app/menu.c app/bindings.c app/settings.c
OBJECTS := $(addprefix $(OBJ)/,$(addsuffix .o,$(CORE_C) $(CORE_CPP) $(CORE_ASM) engine/ffight_idle.cpp engine/idle_branch.s $(APP_C)))
OBJECTS += $(DISPLAY_OBJECTS) out/extra/compat.o

.PHONY: all
all: $(APP).3dsx

$(APP).3dsx: $(APP).elf $(APP).smdh
	$(DEVKITPRO)/tools/bin/3dsxtool $< $@ --smdh=$(APP).smdh

$(APP).smdh: assets/icon.png Makefile
	@mkdir -p $(@D)
	$(DEVKITPRO)/tools/bin/smdhtool --create "Astra CPS1" "Arcade CPS1 / Old 3DS" "Astra / PiFBA contributors" $< $@

$(APP).elf: $(OBJECTS)
	$(CXX) $(ARCH) -specs=3dsx.specs -Wl,--gc-sections,-Map,$(APP).map -o $@ $^ -L$(PORTS)/lib -L$(SDK)/lib -lz $(DISPLAY_LIBS) -lctru -lm

$(OBJ)/app/%.c.o: app/%.c Makefile
	@mkdir -p $(@D)
	$(CC) $(COMMON) $(INCLUDES) $(DISPLAY_CFLAGS) -std=gnu11 -Wall -Wextra -MMD -MP -c $< -o $@

$(OBJ)/%.c.o: %.c Makefile build/core.mk
	@mkdir -p $(@D)
	$(CC) $(COMMON) $(INCLUDES) $(CORE_FLAGS) -std=gnu99 -MMD -MP -c $< -o $@

$(OBJ)/%.cpp.o: %.cpp Makefile build/core.mk
	@mkdir -p $(@D)
	$(CXX) $(COMMON) $(INCLUDES) $(CORE_FLAGS) -std=gnu++11 -fno-exceptions -fno-rtti -fpermissive -Wno-write-strings -MMD -MP -c $< -o $@

$(OBJ)/%.s.o: %.s Makefile
	@mkdir -p $(@D)
	$(CC) $(ARCH) -Wa,--no-warn -c $< -o $@

-include $(OBJECTS:.o=.d)
