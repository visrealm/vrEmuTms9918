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

#ifndef _VR_EMU_TMS9918_UTIL_H_
#define _VR_EMU_TMS9918_UTIL_H_

#include "vrEmuTms9918.h"

#include <stddef.h>
#include <string.h>

#define TMS_R0_MODE_GRAPHICS_I    0x00
#define TMS_R0_MODE_GRAPHICS_II   0x02
#define TMS_R0_MODE_MULTICOLOR    0x00
#define TMS_R0_MODE_TEXT          0x00
#define TMS_R0_EXT_VDP_ENABLE     0x01
#define TMS_R0_EXT_VDP_DISABLE    0x00

#define TMS_R1_RAM_16K            0x80
#define TMS_R1_RAM_4K             0x00
#define TMS_R1_DISP_BLANK         0x00
#define TMS_R1_DISP_ACTIVE        0x40
#define TMS_R1_INT_ENABLE         0x20
#define TMS_R1_INT_DISABLE        0x00
#define TMS_R1_MODE_GRAPHICS_I    0x00
#define TMS_R1_MODE_GRAPHICS_II   0x00
#define TMS_R1_MODE_MULTICOLOR    0x08
#define TMS_R1_MODE_TEXT          0x10
#define TMS_R1_SPRITE_8           0x00
#define TMS_R1_SPRITE_16          0x02
#define TMS_R1_SPRITE_MAG1        0x00
#define TMS_R1_SPRITE_MAG2        0x01

#define TMS_DEFAULT_VRAM_NAME_ADDRESS          0x3800
#define TMS_DEFAULT_VRAM_COLOR_ADDRESS         0x0000
#define TMS_DEFAULT_VRAM_PATT_ADDRESS          0x2000
#define TMS_DEFAULT_VRAM_SPRITE_ATTR_ADDRESS   0x3B00
#define TMS_DEFAULT_VRAM_SPRITE_PATT_ADDRESS   0x1800

 /*
  * TMS9918 palette (RGBA)
  */
VR_EMU_TMS9918_DLLEXPORT_CONST uint32_t vrEmuTms9918Palette[];

/*
 * Write a register value
 */
inline static void vrEmuTms9918WriteRegisterValue(VR_EMU_INST_ARG vrEmuTms9918Register reg, uint8_t value)
{
  vrEmuTms9918WriteAddr(VR_EMU_INST value);
  vrEmuTms9918WriteAddr(VR_EMU_INST 0x80 | (uint8_t)reg);
}

/*
 * Set current VRAM address for reading
 */
inline static void vrEmuTms9918SetAddressRead(VR_EMU_INST_ARG uint16_t addr)
{
  vrEmuTms9918WriteAddr(VR_EMU_INST addr & 0x00ff);
  vrEmuTms9918WriteAddr(VR_EMU_INST ((addr & 0xff00) >> 8));
}

/*
 * Set current VRAM address for writing
 */
inline static void vrEmuTms9918SetAddressWrite(VR_EMU_INST_ARG uint16_t addr)
{
  vrEmuTms9918SetAddressRead(VR_EMU_INST addr | 0x4000);
}

/*
 * Write a series of bytes to the VRAM
 */
inline static void vrEmuTms9918WriteBytes(VR_EMU_INST_ARG const uint8_t* bytes, size_t numBytes)
{
  for (size_t i = 0; i < numBytes; ++i)
  {
    vrEmuTms9918WriteData(VR_EMU_INST bytes[i]);
  }
}

/*
 * Write a series of bytes to the VRAM
 */
inline static void vrEmuTms9918WriteByteRpt(VR_EMU_INST_ARG uint8_t byte, size_t rpt)
{
  for (size_t i = 0; i < rpt; ++i)
  {
    vrEmuTms9918WriteData(VR_EMU_INST byte);
  }
}


/*
 * Write a series of chars to the VRAM
 */
inline static void vrEmuTms9918WriteString(VR_EMU_INST_ARG const char* str)
{
  size_t len = strlen(str);
  for (size_t i = 0; i < len; ++i)
  {
    vrEmuTms9918WriteData(VR_EMU_INST str[i]);
  }
}

/*
 * Write a series of chars to the VRAM with offset
 */
inline static void vrEmuTms9918WriteStringOffset(VR_EMU_INST_ARG const char* str, uint8_t offset)
{
  size_t len = strlen(str);
  for (size_t i = 0; i < len; ++i)
  {
    vrEmuTms9918WriteData(VR_EMU_INST str[i] + offset);
  }
}

/*
 * Return a colur byte consisting of foreground and background colors
 */
inline static uint8_t vrEmuTms9918FgBgColor(vrEmuTms9918Color fg, vrEmuTms9918Color bg)
{
  return (uint8_t)((uint8_t)fg << 4) | (uint8_t)bg;
}

/*
 * Set name table address
 */
inline static void vrEmuTms9918SetNameTableAddr(VR_EMU_INST_ARG uint16_t addr)
{
  vrEmuTms9918WriteRegisterValue(VR_EMU_INST TMS_REG_NAME_TABLE, addr >> 10);
}

/*
 * Set color table address
 */
inline static void vrEmuTms9918SetColorTableAddr(VR_EMU_INST_ARG uint16_t addr)
{
  vrEmuTms9918WriteRegisterValue(VR_EMU_INST TMS_REG_COLOR_TABLE, (uint8_t)(addr >> 6));
}

/*
 * Set pattern table address
 */
inline static void vrEmuTms9918SetPatternTableAddr(VR_EMU_INST_ARG uint16_t addr)
{
  vrEmuTms9918WriteRegisterValue(VR_EMU_INST TMS_REG_PATTERN_TABLE, addr >> 11);
}

/*
 * Set sprite attribute table address
 */
inline static void vrEmuTms9918SetSpriteAttrTableAddr(VR_EMU_INST_ARG uint16_t addr)
{
  vrEmuTms9918WriteRegisterValue(VR_EMU_INST TMS_REG_SPRITE_ATTR_TABLE, (uint8_t)(addr >> 7));
}

/*
 * Set sprite pattern table address
 */
inline static void vrEmuTms9918SetSpritePattTableAddr(VR_EMU_INST_ARG uint16_t addr)
{
  vrEmuTms9918WriteRegisterValue(VR_EMU_INST TMS_REG_SPRITE_PATT_TABLE, addr >> 11);
}

/*
 * Set foreground (text mode) and background colors
 */
inline static void vrEmuTms9918SetFgBgColor(VR_EMU_INST_ARG vrEmuTms9918Color fg, vrEmuTms9918Color bg)
{
  vrEmuTms9918WriteRegisterValue(VR_EMU_INST TMS_REG_FG_BG_COLOR, vrEmuTms9918FgBgColor(fg, bg));
}


/*
 * Initialise for Graphics I mode
 */
VR_EMU_TMS9918_DLLEXPORT
void vrEmuTms9918InitialiseGfxI(VR_EMU_INST_ONLY_ARG);

/*
 * Initialise for Graphics II mode
 */
VR_EMU_TMS9918_DLLEXPORT
void vrEmuTms9918InitialiseGfxII(VR_EMU_INST_ONLY_ARG);

#endif // _VR_EMU_TMS9918_UTIL_H_
