/*
 * Troy's TMS9918 Emulator - Core interface
 *
 * Copyright (c) 2021 Troy Schrapel
 *
 * This code is licensed under the MIT license
 *
 * https://github.com/visrealm/VrEmuTms9918a
 *
 */

#include "tms9918_core.h"
#include <stdlib.h>
#include <memory.h>
#include <math.h>

#define VRAM_SIZE (1 << 14) /* 16KB */

#define NUM_REGISTERS 8

#define GRAPHICS_NUM_COLS 32
#define GRAPHICS_NUM_ROWS 24
#define GRAPHICS_CHAR_WIDTH 8

#define TEXT_NUM_COLS 40
#define TEXT_NUM_ROWS 24
#define TEXT_CHAR_WIDTH 6

#define MAX_SPRITES 32
#define SPRITE_ATTR_BYTES 4
#define LAST_SPRITE_VPOS 0xD0


 /* PRIVATE DATA STRUCTURE
  * ---------------------------------------- */
struct vrEmuTMS9918a_s
{
  byte vram[VRAM_SIZE];

  byte registers[NUM_REGISTERS];

  byte lastMode;

  unsigned short currentAddress;

  vrEmuTms9918aMode mode;
};


/* Function:  tmsMode
  * --------------------
  * return the current mode
  */
static vrEmuTms9918aMode tmsMode(VrEmuTms9918a* tms9918a)
{
  if (tms9918a->registers[0] & 0x02)
  {
    return TMS_MODE_GRAPHICS_II;
  }

  switch ((tms9918a->registers[1] & 0x18) >> 3)
  {
    case 0:
      return TMS_MODE_GRAPHICS_I;

    case 1:
      return TMS_MODE_MULTICOLOR;

    case 2:
      return TMS_MODE_TEXT;
  }
  return TMS_MODE_GRAPHICS_I;
}

/* Function:  tmsDisplayEnabled
  * --------------------
  * check BLANK flag
  */
static inline int tmsDisplayEnabled(VrEmuTms9918a* tms9918a)
{
  return (tms9918a->registers[1] & 0x40) ? 1 : 0;
}


/* Function:  tmsSpriteSize
  * --------------------
  * sprite size (0 = 8x8, 1 = 16x16)
  */
static inline int tmsSpriteSize(VrEmuTms9918a* tms9918a)
{
  return (tms9918a->registers[1] & 0x02) >> 1;
}

/* Function:  tmsSpriteMagnification
  * --------------------
  * sprite size (0 = 1x, 1 = 2x)
  */
static inline int tmsSpriteMag(VrEmuTms9918a* tms9918a)
{
  return tms9918a->registers[1] & 0x01;
}

/* Function:  tmsNameTableAddr
  * --------------------
  * name table base address
  */
static inline unsigned short tmsNameTableAddr(VrEmuTms9918a* tms9918a)
{
  return (tms9918a->registers[2] & 0x0f) << 10;
}

/* Function:  tmsColorTableAddr
  * --------------------
  * color table base address
  */
static inline unsigned short tmsColorTableAddr(VrEmuTms9918a* tms9918a)
{
  return tms9918a->registers[3] << 6;
}

/* Function:  tmsPatternTableAddr
  * --------------------
  * pattern table base address
  */
static inline unsigned short tmsPatternTableAddr(VrEmuTms9918a* tms9918a)
{
  return (tms9918a->registers[4] & 0x07) << 11;
}

/* Function:  tmsSpriteAttrTableAddr
  * --------------------
  * sprite attribute table base address
  */
static inline unsigned short tmsSpriteAttrTableAddr(VrEmuTms9918a* tms9918a)
{
  return (tms9918a->registers[5] & 0x7f) << 7;
}

/* Function:  tmsSpritePatternTableAddr
  * --------------------
  * sprite pattern table base address
  */
static inline unsigned short tmsSpritePatternTableAddr(VrEmuTms9918a* tms9918a)
{
  return (tms9918a->registers[6] & 0x07) << 11;
}

/* Function:  tmsFgColor
  * --------------------
  * foreground color
  */
static inline vrEmuTms9918aColor tmsFgColor(VrEmuTms9918a* tms9918a)
{
  return (vrEmuTms9918aColor)((tms9918a->registers[7] & 0xf0) >> 4);
}

/* Function:  tmsBgColor
  * --------------------
  * background color
  */
static inline vrEmuTms9918aColor tmsBgColor(VrEmuTms9918a* tms9918a)
{
  return (vrEmuTms9918aColor)(tms9918a->registers[7] & 0x0f);
}


/* Function:  vrEmuTms9918aNew
  * --------------------
  * create a new TMS9918A
  */
VR_EMU_TMS9918A_DLLEXPORT VrEmuTms9918a* vrEmuTms9918aNew()
{
  VrEmuTms9918a* tms9918a = (VrEmuTms9918a*)malloc(sizeof(VrEmuTms9918a));
  if (tms9918a != NULL)
  {
    /* initialization */
    tms9918a->currentAddress = 0;
    tms9918a->lastMode = 0;
    memset(tms9918a->registers, 0, sizeof(tms9918a->registers));
    memset(tms9918a->vram, 85, sizeof(tms9918a->vram));
  }

  return tms9918a;
}

/* Function:  vrEmuTms9918aDestroy
 * --------------------
 * destroy a TMS9918A
 *
 * tms9918a: tms9918a object to destroy / clean up
 */
VR_EMU_TMS9918A_DLLEXPORT void vrEmuTms9918aDestroy(VrEmuTms9918a* tms9918a)
{
  if (tms9918a)
  {
    /* destruction */
    free(tms9918a);
  }
}

/* Function:  vrEmuTms9918aWriteAddr
 * --------------------
 * write an address (mode = 1) to the tms9918a
 *
 * byte: the data (DB0 -> DB7) to send
 */
VR_EMU_TMS9918A_DLLEXPORT void vrEmuTms9918aWriteAddr(VrEmuTms9918a* tms9918a, byte data)
{
  if (tms9918a->lastMode)
  {
    /* second address byte */

    if (data & 0x80) /* register */
    {
      tms9918a->registers[data & 0x07] = tms9918a->currentAddress & 0xff;

      tms9918a->mode = tmsMode(tms9918a);
    }
    else /* address */
    {
      tms9918a->currentAddress |= ((data & 0x3f) << 8);
    }
    tms9918a->lastMode = 0;
  }
  else
  {
    tms9918a->currentAddress = data;
    tms9918a->lastMode = 1;
  }
}

/* Function:  vrEmuTms9918aWriteData
 * --------------------
 * write data (mode = 0) to the tms9918a
 *
 * byte: the data (DB0 -> DB7) to send
 */
VR_EMU_TMS9918A_DLLEXPORT void vrEmuTms9918aWriteData(VrEmuTms9918a* tms9918a, byte data)
{
  tms9918a->vram[(tms9918a->currentAddress++) & 0x3fff] = data;
}

/* Function:  vrEmuTms9918aReadStatus
 * --------------------
 * read from the status register
 */
VR_EMU_TMS9918A_DLLEXPORT byte vrEmuTms9918aReadStatus(VrEmuTms9918a* tms9918a)
{
  return 0;
}

/* Function:  vrEmuTms9918aReadData
 * --------------------
 * write data (mode = 0) to the tms9918a
 *
 * byte: the data (DB0 -> DB7) to send
 */
VR_EMU_TMS9918A_DLLEXPORT byte vrEmuTms9918aReadData(VrEmuTms9918a* tms9918a)
{
  return tms9918a->vram[(tms9918a->currentAddress++) & 0x3fff];
}


/* Function:  vrEmuTms9918aOutputSprites
 * ----------------------------------------
 * Output Sprites to a scanline
 */
static void vrEmuTms9918aOutputSprites(VrEmuTms9918a* tms9918a, byte y, byte pixels[TMS9918A_PIXELS_X])
{
  int spriteSizePx = (tmsSpriteSize(tms9918a) ? 16 : 8) * (tmsSpriteMag(tms9918a) ? 2 : 1);
  unsigned short spriteAttrTableAddr = tmsSpriteAttrTableAddr(tms9918a);
  unsigned short spritePatternAddr = tmsSpritePatternTableAddr(tms9918a);

  for (int i = 0; i < MAX_SPRITES; ++i)
  {
    int spriteAttrAddr = spriteAttrTableAddr + i * SPRITE_ATTR_BYTES;

    int vPos = tms9918a->vram[spriteAttrAddr];

    /* stop processing when vPos == LAST_SPRITE_VPOS */
    if (vPos == LAST_SPRITE_VPOS)
      break;

    /* check if sprite position is in the -31 to 0 range */
    if (vPos > (byte)-32)
    {
      vPos -= 256;
    }

    vPos += 1;

    int patternRow = y - vPos;
    if (tmsSpriteMag(tms9918a))
    {
      patternRow /= 2;
    }

    /* check if sprite is visible on this line */
    if (patternRow < 0 || patternRow >= (tmsSpriteSize(tms9918a) ? 16 : 8))
      continue;

    vrEmuTms9918aColor spriteColor = tms9918a->vram[spriteAttrAddr + 3] & 0x0f;
    if (spriteColor == TMS_TRANSPARENT)
      continue;

    /* sprite is visible on this line */
    byte patternName = tms9918a->vram[spriteAttrAddr + 2];

    unsigned short patternOffset = patternName * 8 + patternRow;

    int hPos = tms9918a->vram[spriteAttrAddr + 1];
    if (tms9918a->vram[spriteAttrAddr + 3] & 0x80)  /* check early clock bit */
    {
      hPos -= 32;
    }

    byte patternByte = tms9918a->vram[patternOffset];

    int screenBit  = 0;
    int patternBit = 0;

    for (int screenX = hPos; screenX < (hPos + spriteSizePx); ++screenX, ++screenBit)
    {
      if (screenX >= TMS9918A_PIXELS_X)
      {
        break;
      }

      if (screenX >= 0)
      {
        if (patternByte & (0x80 >> patternBit))
        {
          pixels[screenX] = spriteColor;
        }
      }

      if (!tmsSpriteMag(tms9918a) || (screenBit & 0x01))
      {
        if (++patternBit == 8) /* from A -> C or B -> D of large sprite */
        {
          patternBit = 0;
          patternByte = tms9918a->vram[spritePatternAddr+patternOffset + 16];
        }
      }
    }    
  }

}


/* Function:  vrEmuTms9918aGraphicsIScanLine
 * ----------------------------------------
 * generate a Graphics I mode scanline
 */
static void vrEmuTms9918aGraphicsIScanLine(VrEmuTms9918a* tms9918a, byte y, byte pixels[TMS9918A_PIXELS_X])
{
  int textRow = y / 8;
  int patternRow = y % 8;

  unsigned short namesAddr = tmsNameTableAddr(tms9918a) + textRow * GRAPHICS_NUM_COLS;

  unsigned short patternBaseAddr = tmsPatternTableAddr(tms9918a);
  unsigned short colorBaseAddr = tmsColorTableAddr(tms9918a);

  int pixelIndex = -1;

  for (int tileX = 0; tileX < GRAPHICS_NUM_COLS; ++tileX)
  {
    int pattern = tms9918a->vram[namesAddr + tileX];
    
    byte patternByte = tms9918a->vram[patternBaseAddr + pattern * 8 + patternRow];

    byte colorByte = tms9918a->vram[colorBaseAddr + pattern / 8];

    vrEmuTms9918aColor fgColor = (vrEmuTms9918aColor)((colorByte & 0xf0) >> 4);
    vrEmuTms9918aColor bgColor = (vrEmuTms9918aColor)(colorByte & 0x0f);

    for (int i = 0; i < GRAPHICS_CHAR_WIDTH; ++i)
    {
      pixels[++pixelIndex] = (patternByte & 0x80) ? fgColor : bgColor;
      patternByte <<= 1;
    }
  }

  vrEmuTms9918aOutputSprites(tms9918a, y, pixels);
}

/* Function:  vrEmuTms9918aGraphicsIIScanLine
 * ----------------------------------------
 * generate a Graphics II mode scanline
 */
static void vrEmuTms9918aGraphicsIIScanLine(VrEmuTms9918a* tms9918a, byte y, byte pixels[TMS9918A_PIXELS_X])
{
  int textRow = y / 8;
  int patternRow = y % 8;

  unsigned short namesAddr = tmsNameTableAddr(tms9918a) + textRow * GRAPHICS_NUM_COLS;

  int pageThird = (textRow & 0x18) >> 3; /* which page? 0-2 */
  int pageOffset = pageThird << 11;       /* offset (0, 0x800 or 0x1000) */

  unsigned short patternBaseAddr = tmsPatternTableAddr(tms9918a) + pageOffset;
  unsigned short colorBaseAddr = tmsColorTableAddr(tms9918a) + pageOffset;

  int pixelIndex = -1;

  for (int tileX = 0; tileX < GRAPHICS_NUM_COLS; ++tileX)
  {
    int pattern = tms9918a->vram[namesAddr + tileX];

    byte patternByte = tms9918a->vram[patternBaseAddr + pattern * 8 + patternRow];
    byte colorByte = tms9918a->vram[colorBaseAddr + pattern * 8 + patternRow];

    vrEmuTms9918aColor fgColor = (vrEmuTms9918aColor)((colorByte & 0xf0) >> 4);
    vrEmuTms9918aColor bgColor = (vrEmuTms9918aColor)(colorByte & 0x0f);

    for (int i = 0; i < GRAPHICS_CHAR_WIDTH; ++i)
    {
      pixels[++pixelIndex] = (patternByte & 0x80) ? fgColor : bgColor;
      patternByte <<= 1;
    }
  }

  vrEmuTms9918aOutputSprites(tms9918a, y, pixels);
}

/* Function:  vrEmuTms9918aTextScanLine
 * ----------------------------------------
 * generate a Text mode scanline
 */
static void vrEmuTms9918aTextScanLine(VrEmuTms9918a* tms9918a, byte y, byte pixels[TMS9918A_PIXELS_X])
{
  int textRow = y / 8;
  int patternRow = y % 8;

  unsigned short namesAddr = tmsNameTableAddr(tms9918a) + textRow * TEXT_NUM_COLS;

  vrEmuTms9918aColor bgColor = tmsBgColor(tms9918a);
  vrEmuTms9918aColor fgColor = tmsFgColor(tms9918a);

  int pixelIndex = -1;
  for (int tileX = 0; tileX < TEXT_NUM_COLS; ++tileX)
  {
    int pattern = tms9918a->vram[namesAddr + tileX];

    byte patternByte = tms9918a->vram[tmsPatternTableAddr(tms9918a) + pattern * 8 + patternRow];

    byte colorByte = tms9918a->vram[tmsColorTableAddr(tms9918a) + pattern / 8];

    for (int i = 0; i < TEXT_CHAR_WIDTH; ++i)
    {
      pixels[++pixelIndex] = (patternByte & 0x80) ? fgColor : bgColor;
      patternByte <<= 1;
    }
  }
}

/* Function:  vrEmuTms9918aMulticolorScanLine
 * ----------------------------------------
 * generate a Multicolor mode scanline
 */
static void vrEmuTms9918aMulticolorScanLine(VrEmuTms9918a* tms9918a, byte y, byte pixels[TMS9918A_PIXELS_X])
{

}


/* Function:  vrEmuTms9918aScanLine
 * ----------------------------------------
 * generate a scanline
 */
VR_EMU_TMS9918A_DLLEXPORT void vrEmuTms9918aScanLine(VrEmuTms9918a* tms9918a, byte y, byte pixels[TMS9918A_PIXELS_X])
{
  if (tms9918a == NULL)
    return;

  if (!tmsDisplayEnabled(tms9918a))
  {
    memset(pixels, TMS_BLACK, TMS9918A_PIXELS_X);
    return;
  }

  if (y >= TMS9918A_PIXELS_Y)
  {
    memset(pixels, tmsBgColor(tms9918a), TMS9918A_PIXELS_X);
    return;
  }

  switch (tms9918a->mode)
  {
    case TMS_MODE_GRAPHICS_I:
      vrEmuTms9918aGraphicsIScanLine(tms9918a, y, pixels);
      break;

    case TMS_MODE_GRAPHICS_II:
      vrEmuTms9918aGraphicsIIScanLine(tms9918a, y, pixels);
      break;

    case TMS_MODE_TEXT:
      vrEmuTms9918aTextScanLine(tms9918a, y, pixels);
      break;

    case TMS_MODE_MULTICOLOR:
      vrEmuTms9918aMulticolorScanLine(tms9918a, y, pixels);
      break;
  }
}

/* Function:  vrEmuTms9918aRegValue
 * ----------------------------------------
 * return a reigister value
 */
VR_EMU_TMS9918A_DLLEXPORT
byte vrEmuTms9918aRegValue(VrEmuTms9918a * tms9918a, byte reg)
{
  return tms9918a->registers[reg & 0x07];
}

/* Function:  vrEmuTms9918aVramValue
 * ----------------------------------------
 * return a value from vram
 */
VR_EMU_TMS9918A_DLLEXPORT
byte vrEmuTms9918aVramValue(VrEmuTms9918a* tms9918a, unsigned short addr)
{
  return tms9918a->vram[addr & 0x3fff];
}


