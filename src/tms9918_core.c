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

  switch (tms9918a->registers[1] & 0x18 >> 3)
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

/* Function:  tmsSpriteSize
  * --------------------
  * sprite size (0 = 8x8, 1 = 16x16)
  */
static int tmsSpriteSize(VrEmuTms9918a* tms9918a)
{
  return (tms9918a->registers[1] & 0x02) >> 1;
}

/* Function:  tmsSpriteMagnification
  * --------------------
  * sprite size (0 = 1x, 1 = 2x)
  */
static int tmsSpriteMag(VrEmuTms9918a* tms9918a)
{
  return tms9918a->registers[1] & 0x01;
}

/* Function:  tmsNameTableAddr
  * --------------------
  * name table base address
  */
static unsigned short tmsNameTableAddr(VrEmuTms9918a* tms9918a)
{
  return tms9918a->registers[2] << 10;
}

/* Function:  tmsColorTableAddr
  * --------------------
  * color table base address
  */
static unsigned short tmsColorTableAddr(VrEmuTms9918a* tms9918a)
{
  return tms9918a->registers[3] << 6;
}

/* Function:  tmsPatternTableAddr
  * --------------------
  * pattern table base address
  */
static unsigned short tmsPatternTableAddr(VrEmuTms9918a* tms9918a)
{
  return tms9918a->registers[4] << 11;
}

/* Function:  tmsSpriteAttrTableAddr
  * --------------------
  * sprite attribute table base address
  */
static unsigned short tmsSpriteAttrTableAddr(VrEmuTms9918a* tms9918a)
{
  return tms9918a->registers[5] << 7;
}

/* Function:  tmsSpritePatternTableAddr
  * --------------------
  * sprite pattern table base address
  */
static unsigned short tmsSpritePatternTableAddr(VrEmuTms9918a* tms9918a)
{
  return tms9918a->registers[6] << 11;
}

/* Function:  tmsFgColor
  * --------------------
  * foreground color
  */
static vrEmuTms9918aColor tmsFgColor(VrEmuTms9918a* tms9918a)
{
  return (vrEmuTms9918aColor)((tms9918a->registers[7] & 0xf0) >> 4);
}

/* Function:  tmsBgColor
  * --------------------
  * background color
  */
static vrEmuTms9918aColor tmsBgColor(VrEmuTms9918a* tms9918a)
{
  return (vrEmuTms9918aColor)(tms9918a->registers[7] & 0x0f);
}


/* Function:  vrEmuTms9918aNew
  * --------------------
  * create a new TMS9918A
  */
VR_EMU_TMS9918A_DLLEXPORT VrEmuTms9918a* VrEmuTms9918aNew()
{
  VrEmuTms9918a* tms9918a = (VrEmuTms9918a*)malloc(sizeof(VrEmuTms9918a));
  if (tms9918a != NULL)
  {
    /* initialization */
    tms9918a->currentAddress = 0;
    tms9918a->lastMode = 0;
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
      tms9918a->currentAddress |= (data & 0x3f) << 8;
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
  tms9918a->vram[tms9918a->currentAddress++] = data;
  tms9918a->lastMode = 0;
}

/* Function:  vrEmuTms9918aReadStatus
 * --------------------
 * read from the status register
 */
VR_EMU_TMS9918A_DLLEXPORT byte vrEmuTms9918aReadStatus(VrEmuTms9918a* tms9918a)
{
  tms9918a->lastMode = 0;
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
  tms9918a->lastMode = 0;
  return tms9918a->vram[tms9918a->currentAddress++];
}


vrEmuTms9918aOutputSprites(VrEmuTms9918a* tms9918a, byte y, vrEmuTms9918aColor pixels[TMS9918A_PIXELS_X])
{

}


vrEmuTms9918aGraphicsIScanLine(VrEmuTms9918a* tms9918a, byte y, vrEmuTms9918aColor pixels[TMS9918A_PIXELS_X])
{
  int textRow = y / 8;
  int patternRow = y % 8;

  unsigned short namesAddr = tmsNameTableAddr(tms9918a) + textRow * GRAPHICS_NUM_COLS;

  int pixelIndex = 0;

  for (int tileX = 0; tileX < GRAPHICS_NUM_COLS; ++tileX)
  {
    int pattern = tms9918a->vram[namesAddr + tileX];
    
    byte patternByte = tms9918a->vram[tmsPatternTableAddr(tms9918a) + patternRow];

    byte colorByte = tms9918a->vram[tmsColorTableAddr(tms9918a) + pattern / 8];

    vrEmuTms9918aColor bgColor = (vrEmuTms9918aColor)((colorByte & 0xf0) >> 4);
    vrEmuTms9918aColor fgColor = (vrEmuTms9918aColor)(colorByte & 0x0f);

    pixels[pixelIndex++] = patternByte & 0x80 ? fgColor : bgColor;
    pixels[pixelIndex++] = patternByte & 0x40 ? fgColor : bgColor;
    pixels[pixelIndex++] = patternByte & 0x20 ? fgColor : bgColor;
    pixels[pixelIndex++] = patternByte & 0x10 ? fgColor : bgColor;
    pixels[pixelIndex++] = patternByte & 0x08 ? fgColor : bgColor;
    pixels[pixelIndex++] = patternByte & 0x04 ? fgColor : bgColor;
    pixels[pixelIndex++] = patternByte & 0x02 ? fgColor : bgColor;
    pixels[pixelIndex++] = patternByte & 0x01 ? fgColor : bgColor;
  }

  vrEmuTms9918aOutputSprites(tms9918a, y, pixels);
}

vrEmuTms9918aGraphicsIIScanLine(VrEmuTms9918a* tms9918a, byte y, vrEmuTms9918aColor pixels[TMS9918A_PIXELS_X])
{

}

vrEmuTms9918aTextScanLine(VrEmuTms9918a* tms9918a, byte y, vrEmuTms9918aColor pixels[TMS9918A_PIXELS_X])
{

}

vrEmuTms9918aMulticolorScanLine(VrEmuTms9918a* tms9918a, byte y, vrEmuTms9918aColor pixels[TMS9918A_PIXELS_X])
{

}


/* Function:  vrEmuTms9918aScanLine
 * ----------------------------------------
 * generate a scanline
 */
VR_EMU_TMS9918A_DLLEXPORT void vrEmuTms9918aScanLine(VrEmuTms9918a* tms9918a, byte y, vrEmuTms9918aColor pixels[TMS9918A_PIXELS_X])
{
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

