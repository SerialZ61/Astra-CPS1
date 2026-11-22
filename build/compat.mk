# Isolate the supplementary revision drivers so each engine owns its emulator state.
FBA := vendor/fba2012-cps1
EXTRA_DIRS := $(FBA)/burn $(FBA)/burn/devices $(FBA)/burn/snd $(FBA)/burn/drv/capcom $(FBA)/cpu $(FBA)/cpu/z80 $(FBA)/cpu/m68k
EXTRA_SRC := $(foreach d,$(EXTRA_DIRS),$(wildcard $(d)/*.cpp $(d)/*.c)) engine/compat/bridge.cpp
EXTRA_OBJ := $(addprefix out/extra/,$(addsuffix .o,$(EXTRA_SRC)))
EXTRA_FLAGS := $(ARCH) -O2 -fomit-frame-pointer -fno-strict-aliasing -fwrapv -ffunction-sections -fdata-sections -mword-relocations -DEMU_M68K -DFBACORES_CPS -D__LIBRETRO__ -D__LIBRETRO_OPTIMIZATIONS__ -DFRONTEND_SUPPORTS_RGB565 -DNDEBUG -Iengine/compat -I$(FBA)/burner/libretro -I$(FBA)/dep/generated $(addprefix -I,$(EXTRA_DIRS)) -I$(SDK)/include
out/extra/%.cpp.o: %.cpp build/compat.mk
	@mkdir -p $(@D)
	$(CXX) $(EXTRA_FLAGS) -Wno-write-strings -fpermissive -fno-rtti -fno-exceptions -std=gnu++11 -MMD -MP -c $< -o $@
out/extra/%.c.o: %.c build/compat.mk
	@mkdir -p $(@D)
	$(CC) $(EXTRA_FLAGS) -std=gnu99 -MMD -MP -c $< -o $@
out/extra/compat.o: $(EXTRA_OBJ)
	$(DEVKITARM)/bin/arm-none-eabi-ld -r --gc-sections -u astra_extra_api -o $@.tmp $^
	$(DEVKITARM)/bin/arm-none-eabi-objcopy --keep-global-symbol=astra_extra_api $@.tmp $@
-include $(EXTRA_OBJ:.o=.d)
