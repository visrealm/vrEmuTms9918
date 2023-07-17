/*
 * Troy's TMS9918 Emulator - Utility / helper functions
 *
 * Copyright (c) 2022 Troy Schrapel
 *
 * This code is licensed under the MIT license
 *
 * https://github.com/visrealm/vrEmuTms9918
 *
 */

#include "vrEmuTms9918Util.h"

 /* tms9918 palette */
VR_EMU_TMS9918_DLLEXPORT_C
const uint32_t vrEmuTms9918Palette[] = {
  0x00000000, /* transparent */
  0x000000ff, /* black */
  0x21c942ff, /* medium green */
  0x5edc78ff, /* light green */
  0x5455edff, /* dark blue */
  0x7d75fcff, /* light blue */
  0xd3524dff, /* dark red */
  0x43ebf6ff, /* cyan */
  0xfd5554ff, /* medium red */
  0xff7978ff, /* light red */
  0xd3c153ff, /* dark yellow */
  0xe5ce80ff, /* light yellow */
  0x21b03cff, /* dark green */
  0xc95bbaff, /* magenta */
  0xccccccff, /* grey */
  0xffffffff  /* white */
};


VR_EMU_TMS9918_DLLEXPORT_C
void vrEmuTms9918InitialiseGfxI(VrEmuTms9918* tms9918)
{
  vrEmuTms9918WriteRegisterValue(tms9918, TMS_REG_0, TMS_R0_EXT_VDP_DISABLE | TMS_R0_MODE_GRAPHICS_I);
  vrEmuTms9918WriteRegisterValue(tms9918, TMS_REG_1, TMS_R1_RAM_16K | TMS_R1_MODE_GRAPHICS_I | TMS_R1_RAM_16K | TMS_R1_DISP_ACTIVE | TMS_R1_INT_ENABLE);
  vrEmuTms9918SetNameTableAddr(tms9918, TMS_DEFAULT_VRAM_NAME_ADDRESS);
  vrEmuTms9918SetColorTableAddr(tms9918, TMS_DEFAULT_VRAM_COLOR_ADDRESS);
  vrEmuTms9918SetPatternTableAddr(tms9918, TMS_DEFAULT_VRAM_PATT_ADDRESS);
  vrEmuTms9918SetSpriteAttrTableAddr(tms9918, TMS_DEFAULT_VRAM_SPRITE_ATTR_ADDRESS);
  vrEmuTms9918SetSpritePattTableAddr(tms9918, TMS_DEFAULT_VRAM_SPRITE_PATT_ADDRESS);
  vrEmuTms9918SetFgBgColor(tms9918, TMS_BLACK, TMS_CYAN);

  vrEmuTms9918SetAddressWrite(tms9918, 0x0000);
  vrEmuTms9918WriteByteRpt(tms9918, 0x00, 0x4000);
}