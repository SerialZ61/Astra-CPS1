# CPS1 emulation library. Paths are relative to the project root.
PI = vendor/pifba
BURN = $(PI)/burn
CPS = $(BURN)/capcom
CORE_INCLUDES = -Iengine -I$(BURN) -I$(CPS) -I$(PI)/cpu/cyclone -I$(PI)/cpu/z80 -I$(PI)/cpu/cz80 -I$(PI)/rpi
CORE_FLAGS = -DASTRA_CPS1 -DOOPSWARE_FIX -D__cdecl= -D__fastcall= -D_T= -fno-strict-aliasing -fwrapv -fno-delete-null-pointer-checks
CORE_C = $(BURN)/ym2151.c $(PI)/cpu/cz80/cz80.c $(PI)/rpi/unzip.c
CORE_ASM = $(PI)/cpu/cyclone/cyclone.s
CORE_CPP = engine/cps1.cpp engine/catalog.cpp engine/dips.cpp engine/romzip.cpp engine/sf2_scroll.cpp $(PI)/cpu/z80/z80.cpp $(PI)/cpu/z80/z80daisy.cpp \
  $(addprefix $(BURN)/,burn.cpp load.cpp sek.cpp zet.cpp burn_sound.cpp burn_sound_c.cpp \
    burn_ym2151.cpp eeprom_93cxx.cpp msm6295.cpp timer.cpp) \
  $(addprefix $(CPS)/,cps.cpp cps_draw.cpp cps_mem.cpp cps_obj.cpp cps_pal.cpp cps_run.cpp \
    cps_rw.cpp cps_scr.cpp cpsr.cpp cpsrd.cpp cpst.cpp ctv.cpp dc_ffight.cpp \
    dc_sf2.cpp dc_sf2t.cpp dc_captcomm.cpp dc_knights.cpp dc_kod.cpp dc_msword.cpp \
    dc_mtwins.cpp dc_nemo.cpp dc_unsquad.cpp dc_willow.cpp dc_ghouls.cpp dc_strider.cpp dc_3wonders.cpp \
    dc_1941.cpp dc_cawing.cpp dc_cworld2j.cpp dc_dino.cpp dc_dw.cpp dc_forgottn.cpp dc_megaman.cpp dc_mercs.cpp dc_pang3.cpp \
    dc_pnickj.cpp dc_punisher.cpp dc_qad.cpp dc_qtono2.cpp dc_sfzch.cpp dc_slammast.cpp dc_varth.cpp dc_wof.cpp kabuki.cpp \
    ps.cpp ps_m.cpp ps_z.cpp qs.cpp qs_c.cpp qs_z.cpp)
