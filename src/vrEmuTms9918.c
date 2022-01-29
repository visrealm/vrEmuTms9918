/*
 * Troy's TMS9918 Emulator - Core interface
 *
 * Copyright (c) 2021 Troy Schrapel
 *
 * This code is licensed under the MIT license
 *
 * https://github.com/visrealm/VrEmuTms9918
 *
 */

#include "vrEmuTms9918.h"
#include <stdlib.h>
#include <memory.h>
#include <math.h>

#define VRAM_SIZE (1 << 14) /* 16KB */

#define GRAPHICS_NUM_COLS 32
#define GRAPHICS_NUM_ROWS 24
#define GRAPHICS_CHAR_WIDTH 8

#define TEXT_NUM_COLS 40
#define TEXT_NUM_ROWS 24
#define TEXT_CHAR_WIDTH 6

#define MAX_SPRITES 32
#define SPRITE_ATTR_BYTES 4
#define LAST_SPRITE_VPOS 0xD0
#define MAX_SCANLINE_SPRITES 4

#define STATUS_INT 0x80
#define STATUS_5S  0x40
#define STATUS_COL 0x20

 /* PRIVATE DATA STRUCTURE
  * ---------------------------------------- */
struct vrEmuTMS9918_s
{
  uint8_t vram[VRAM_SIZE];

  uint8_t registers[TMS_NUM_REGISTERS];
  
  uint8_t status;

  uint8_t lastMode;

  unsigned short currentAddress;

  vrEmuTms9918Mode mode;

  uint8_t rowSpriteBits[TMS9918_PIXELS_X];
};


/* Function:  tmsMode
  * --------------------
  * return the current mode
  */
static vrEmuTms9918Mode tmsMode(VrEmuTms9918* tms9918)
{
  if (tms9918->registers[TMS_REG_0] & 0x02)
  {
    return TMS_MODE_GRAPHICS_II;
  }

  switch ((tms9918->registers[TMS_REG_1] & 0x18) >> 3)
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
static inline int tmsSpriteSize(VrEmuTms9918* tms9918)
{
  return (tms9918->registers[TMS_REG_1] & 0x02) >> 1;
}

/* Function:  tmsSpriteMagnification
  * --------------------
  * sprite size (0 = 1x, 1 = 2x)
  */
static inline int tmsSpriteMag(VrEmuTms9918* tms9918)
{
  return tms9918->registers[TMS_REG_1] & 0x01;
}

/* Function:  tmsNameTableAddr
  * --------------------
  * name table base address
  */
static inline unsigned short tmsNameTableAddr(VrEmuTms9918* tms9918)
{
  return (tms9918->registers[TMS_REG_2] & 0x0f) << 10;
}

/* Function:  tmsColorTableAddr
  * --------------------
  * color table base address
  */
static inline unsigned short tmsColorTableAddr(VrEmuTms9918* tms9918)
{
  if (tms9918->mode == TMS_MODE_GRAPHICS_II)
    return (tms9918->registers[TMS_REG_3] & 0x80) << 6;
  return tms9918->registers[TMS_REG_3] << 6;
}

/* Function:  tmsPatternTableAddr
  * --------------------
  * pattern table base address
  */
static inline unsigned short tmsPatternTableAddr(VrEmuTms9918* tms9918)
{
  if (tms9918->mode == TMS_MODE_GRAPHICS_II)
    return (tms9918->registers[TMS_REG_4] & 0x04) << 11;
  return (tms9918->registers[TMS_REG_4] & 0x07) << 11;
}

/* Function:  tmsSpriteAttrTableAddr
  * --------------------
  * sprite attribute table base address
  */
static inline unsigned short tmsSpriteAttrTableAddr(VrEmuTms9918* tms9918)
{
  return (tms9918->registers[TMS_REG_5] & 0x7f) << 7;
}

/* Function:  tmsSpritePatternTableAddr
  * --------------------
  * sprite pattern table base address
  */
static inline unsigned short tmsSpritePatternTableAddr(VrEmuTms9918* tms9918)
{
  return (tms9918->registers[TMS_REG_6] & 0x07) << 11;
}

/* Function:  tmsBgColor
  * --------------------
  * background color
  */
static inline vrEmuTms9918Color tmsMainBgColor(VrEmuTms9918* tms9918)
{
  return (vrEmuTms9918Color)((vrEmuTms9918DisplayEnabled(tms9918) 
            ? tms9918->registers[TMS_REG_7] 
            : TMS_BLACK) & 0x0f);
}

/* Function:  tmsFgColor
  * --------------------
  * foreground color
  */
static inline vrEmuTms9918Color tmsMainFgColor(VrEmuTms9918* tms9918)
{
  vrEmuTms9918Color c = (vrEmuTms9918Color)(tms9918->registers[TMS_REG_7] >> 4);
  return c == TMS_TRANSPARENT ? tmsMainBgColor(tms9918) : c;
}

/* Function:  tmsFgColor
  * --------------------
  * foreground color
  */
static inline vrEmuTms9918Color tmsFgColor(VrEmuTms9918* tms9918, uint8_t colorByte)
{
  vrEmuTms9918Color c = (vrEmuTms9918Color)(colorByte >> 4);
  return c == TMS_TRANSPARENT ? tmsMainBgColor(tms9918) : c;
}

/* Function:  tmsBgColor
  * --------------------
  * background color
  */
static inline vrEmuTms9918Color tmsBgColor(VrEmuTms9918* tms9918, uint8_t colorByte)
{
  vrEmuTms9918Color c = (vrEmuTms9918Color)(colorByte & 0x0f);
  return c == TMS_TRANSPARENT ? tmsMainBgColor(tms9918) : c;
}


/* Function:  vrEmuTms9918New
  * --------------------
  * create a new TMS9918
  */
VR_EMU_TMS9918_DLLEXPORT VrEmuTms9918* vrEmuTms9918New()
{
  VrEmuTms9918* tms9918 = (VrEmuTms9918*)malloc(sizeof(VrEmuTms9918));
  if (tms9918 != NULL)
  {
    vrEmuTms9918Reset(tms9918);
 }

  return tms9918;
}

/* Function:  vrEmuTms9918Reset
  * --------------------
  * reset the new TMS9918
  */
VR_EMU_TMS9918_DLLEXPORT void vrEmuTms9918Reset(VrEmuTms9918* tms9918)
{
  if (tms9918)
  {
    /* initialization */
    tms9918->currentAddress = 0;
    tms9918->lastMode = 0;
    tms9918->status = 0;
    memset(tms9918->registers, 0, sizeof(tms9918->registers));
    memset(tms9918->vram, 0xff, sizeof(tms9918->vram));
  }
}


/* Function:  vrEmuTms9918Destroy
 * --------------------
 * destroy a TMS9918
 *
 * tms9918: tms9918 object to destroy / clean up
 */
VR_EMU_TMS9918_DLLEXPORT void vrEmuTms9918Destroy(VrEmuTms9918* tms9918)
{
  if (tms9918)
  {
    /* destruction */
    free(tms9918);
  }
}

/* Function:  vrEmuTms9918WriteAddr
 * --------------------
 * write an address (mode = 1) to the tms9918
 *
 * data: the data (DB0 -> DB7) to send
 */
VR_EMU_TMS9918_DLLEXPORT void vrEmuTms9918WriteAddr(VrEmuTms9918* tms9918, uint8_t data)
{
  if (tms9918->lastMode)
  {
    /* second address byte */

    if (data & 0x80) /* register */
    {
      tms9918->registers[data & 0x07] = tms9918->currentAddress & 0xff;

      tms9918->mode = tmsMode(tms9918);
    }
    else /* address */
    {
      tms9918->currentAddress |= ((data & 0x3f) << 8);
    }
    tms9918->lastMode = 0;
  }
  else
  {
    tms9918->currentAddress = data;
    tms9918->lastMode = 1;
  }
}

/* Function:  vrEmuTms9918WriteData
 * --------------------
 * write data (mode = 0) to the tms9918
 *
 * data: the data (DB0 -> DB7) to send
 */
VR_EMU_TMS9918_DLLEXPORT void vrEmuTms9918WriteData(VrEmuTms9918* tms9918, uint8_t data)
{
  tms9918->vram[(tms9918->currentAddress++) & 0x3fff] = data;
}

/* Function:  vrEmuTms9918ReadStatus
 * --------------------
 * read from the status register
 */
VR_EMU_TMS9918_DLLEXPORT uint8_t vrEmuTms9918ReadStatus(VrEmuTms9918* tms9918)
{
  uint8_t tmpStatus = tms9918->status;
  tms9918->status &= ~(STATUS_INT | STATUS_COL);
  return tmpStatus;
}

/* Function:  vrEmuTms9918ReadData
 * --------------------
 * read data (mode = 0) from the tms9918
 */
VR_EMU_TMS9918_DLLEXPORT uint8_t vrEmuTms9918ReadData(VrEmuTms9918* tms9918)
{
  return tms9918->vram[(tms9918->currentAddress++) & 0x3fff];
}

/* Function:  vrEmuTms9918ReadDataNoInc
 * --------------------
 * read data (mode = 0) from the tms9918
 */
VR_EMU_TMS9918_DLLEXPORT uint8_t vrEmuTms9918ReadDataNoInc(VrEmuTms9918* tms9918)
{
  return tms9918->vram[tms9918->currentAddress & 0x3fff];
}

/* Function:  vrEmuTms9918OutputSprites
 * ----------------------------------------
 * Output Sprites to a scanline
 */
static void vrEmuTms9918OutputSprites(VrEmuTms9918* tms9918, uint8_t y, uint8_t pixels[TMS9918_PIXELS_X])
{
  int spriteSizePx = (tmsSpriteSize(tms9918) ? 16 : 8) * (tmsSpriteMag(tms9918) ? 2 : 1);
  unsigned short spriteAttrTableAddr = tmsSpriteAttrTableAddr(tms9918);
  unsigned short spritePatternAddr = tmsSpritePatternTableAddr(tms9918);

  int spritesShown = 0;

  if (y == 0)
  {
    tms9918->status = 0;
  }

  for (int i = 0; i < MAX_SPRITES; ++i)
  {
    int spriteAttrAddr = spriteAttrTableAddr + i * SPRITE_ATTR_BYTES;

    int vPos = tms9918->vram[spriteAttrAddr];

    /* stop processing when vPos == LAST_SPRITE_VPOS */
    if (vPos == LAST_SPRITE_VPOS)
    {
      if ((tms9918->status & STATUS_5S) == 0)
      {
        tms9918->status |= i;
      }
      break;
    }

    /* check if sprite position is in the -31 to 0 range */
    if (vPos > (uint8_t)-32)
    {
      vPos -= 256;
    }

    vPos += 1;

    int patternRow = y - vPos;
    if (tmsSpriteMag(tms9918))
    {
      patternRow /= 2;
    }

    /* check if sprite is visible on this line */
    if (patternRow < 0 || patternRow >= (tmsSpriteSize(tms9918) ? 16 : 8))
      continue;

    vrEmuTms9918Color spriteColor = tms9918->vram[spriteAttrAddr + 3] & 0x0f;

    if (spritesShown == 0)
    {
      /* if we're showing the first sprite, clear the bit buffer */
      memset(tms9918->rowSpriteBits, 0, TMS9918_PIXELS_X);
    }

    /* have we exceeded the scanline sprite limit? */
    if (++spritesShown > MAX_SCANLINE_SPRITES)
    {
      if ((tms9918->status & STATUS_5S) == 0)
      {
        tms9918->status |= STATUS_5S | i;
      }
      break;
    }

    /* sprite is visible on this line */
    uint8_t patternName = tms9918->vram[spriteAttrAddr + 2];

    unsigned short patternOffset = spritePatternAddr + patternName * 8 + patternRow;

    int hPos = tms9918->vram[spriteAttrAddr + 1];
    if (tms9918->vram[spriteAttrAddr + 3] & 0x80)  /* check early clock bit */
    {
      hPos -= 32;
    }

    uint8_t patternByte = tms9918->vram[patternOffset];

    int screenBit  = 0;
    int patternBit = 0;

    for (int screenX = hPos; screenX < (hPos + spriteSizePx); ++screenX, ++screenBit)
    {
      if (screenX >= TMS9918_PIXELS_X)
      {
        break;
      }

      if (screenX >= 0)
      {
        if (patternByte & (0x80 >> patternBit))
        {
          /* we still process transparent sprites, since they're used in 5S and collistion checks */
          if (spriteColor != TMS_TRANSPARENT)
          {
            pixels[screenX] = spriteColor;
          }

          if (tms9918->rowSpriteBits[screenX])
          {
            tms9918->status |= STATUS_COL;
          }
          tms9918->rowSpriteBits[screenX] = 1;
        }
      }

      if (!tmsSpriteMag(tms9918) || (screenBit & 0x01))
      {
        if (++patternBit == 8) /* from A -> C or B -> D of large sprite */
        {
          patternBit = 0;
          patternByte = tms9918->vram[patternOffset + 16];
        }
      }
    }    
  }

}


/* Function:  vrEmuTms9918GraphicsIScanLine
 * ----------------------------------------
 * generate a Graphics I mode scanline
 */
static void vrEmuTms9918GraphicsIScanLine(VrEmuTms9918* tms9918, uint8_t y, uint8_t pixels[TMS9918_PIXELS_X])
{
  int textRow = y / 8;
  int patternRow = y % 8;

  unsigned short namesAddr = tmsNameTableAddr(tms9918) + textRow * GRAPHICS_NUM_COLS;

  unsigned short patternBaseAddr = tmsPatternTableAddr(tms9918);
  unsigned short colorBaseAddr = tmsColorTableAddr(tms9918);

  int pixelIndex = -1;

  for (int tileX = 0; tileX < GRAPHICS_NUM_COLS; ++tileX)
  {
    int pattern = tms9918->vram[namesAddr + tileX];
    
    uint8_t patternByte = tms9918->vram[patternBaseAddr + pattern * 8 + patternRow];

    uint8_t colorByte = tms9918->vram[colorBaseAddr + pattern / 8];

    vrEmuTms9918Color fgColor = tmsFgColor(tms9918, colorByte);
    vrEmuTms9918Color bgColor = tmsBgColor(tms9918, colorByte);

    for (int i = 0; i < GRAPHICS_CHAR_WIDTH; ++i)
    {
      pixels[++pixelIndex] = (patternByte & 0x80) ? fgColor : bgColor;
      patternByte <<= 1;
    }
  }

  vrEmuTms9918OutputSprites(tms9918, y, pixels);
}

/* Function:  vrEmuTms9918GraphicsIIScanLine
 * ----------------------------------------
 * generate a Graphics II mode scanline
 */
static void vrEmuTms9918GraphicsIIScanLine(VrEmuTms9918* tms9918, uint8_t y, uint8_t pixels[TMS9918_PIXELS_X])
{
  int textRow = y / 8;
  int patternRow = y % 8;

  unsigned short namesAddr = tmsNameTableAddr(tms9918) + textRow * GRAPHICS_NUM_COLS;

  int pageThird = (textRow & 0x18) >> 3; /* which page? 0-2 */
  int pageOffset = pageThird << 11;       /* offset (0, 0x800 or 0x1000) */

  unsigned short patternBaseAddr = tmsPatternTableAddr(tms9918) + pageOffset;
  unsigned short colorBaseAddr = tmsColorTableAddr(tms9918) + pageOffset;

  int pixelIndex = -1;

  for (int tileX = 0; tileX < GRAPHICS_NUM_COLS; ++tileX)
  {
    int pattern = tms9918->vram[namesAddr + tileX];

    uint8_t patternByte = tms9918->vram[patternBaseAddr + pattern * 8 + patternRow];
    uint8_t colorByte = tms9918->vram[colorBaseAddr + pattern * 8 + patternRow];

    vrEmuTms9918Color fgColor = tmsFgColor(tms9918, colorByte);
    vrEmuTms9918Color bgColor = tmsBgColor(tms9918, colorByte);

    for (int i = 0; i < GRAPHICS_CHAR_WIDTH; ++i)
    {
      pixels[++pixelIndex] = (patternByte & 0x80) ? fgColor : bgColor;
      patternByte <<= 1;
    }
  }

  vrEmuTms9918OutputSprites(tms9918, y, pixels);
}

/* Function:  vrEmuTms9918TextScanLine
 * ----------------------------------------
 * generate a Text mode scanline
 */
static void vrEmuTms9918TextScanLine(VrEmuTms9918* tms9918, uint8_t y, uint8_t pixels[TMS9918_PIXELS_X])
{
  int textRow = y / 8;
  int patternRow = y % 8;

  unsigned short namesAddr = tmsNameTableAddr(tms9918) + textRow * TEXT_NUM_COLS;

  vrEmuTms9918Color bgColor = tmsMainBgColor(tms9918);
  vrEmuTms9918Color fgColor = tmsMainFgColor(tms9918);
  
  int pixelIndex = -1;

  /* fill the first 8 pixels with bg color */
  while (++pixelIndex < 8)
  {
    pixels[pixelIndex] = bgColor;
  }
  --pixelIndex;

  for (int tileX = 0; tileX < TEXT_NUM_COLS; ++tileX)
  {
    int pattern = tms9918->vram[namesAddr + tileX];

    uint8_t patternByte = tms9918->vram[tmsPatternTableAddr(tms9918) + pattern * 8 + patternRow];

    uint8_t colorByte = tms9918->vram[tmsColorTableAddr(tms9918) + pattern / 8];

    for (int i = 0; i < TEXT_CHAR_WIDTH; ++i)
    {
      pixels[++pixelIndex] = (patternByte & 0x80) ? fgColor : bgColor;
      patternByte <<= 1;
    }
  }

  while (++pixelIndex < TMS9918_PIXELS_X)
  {
    pixels[pixelIndex] = bgColor;
  }
}

/* Function:  vrEmuTms9918MulticolorScanLine
 * ----------------------------------------
 * generate a Multicolor mode scanline
 */
static void vrEmuTms9918MulticolorScanLine(VrEmuTms9918* tms9918, uint8_t y, uint8_t pixels[TMS9918_PIXELS_X])
{
  int textRow = y / 8;
  int patternRow = (y / 4) % 2 + (textRow % 4) * 2;

  unsigned short namesAddr = tmsNameTableAddr(tms9918) + textRow * GRAPHICS_NUM_COLS;

  int pixelIndex = -1;

  for (int tileX = 0; tileX < GRAPHICS_NUM_COLS; ++tileX)
  {
    int pattern = tms9918->vram[namesAddr + tileX];

    uint8_t colorByte = tms9918->vram[tmsPatternTableAddr(tms9918) + pattern * 8 + patternRow];

    for (int i = 0; i < 4; ++i) pixels[++pixelIndex] = tmsFgColor(tms9918, colorByte);
    for (int i = 0; i < 4; ++i) pixels[++pixelIndex] = tmsBgColor(tms9918, colorByte);
  }

  vrEmuTms9918OutputSprites(tms9918, y, pixels);
}


/* Function:  vrEmuTms9918ScanLine
 * ----------------------------------------
 * generate a scanline
 */
VR_EMU_TMS9918_DLLEXPORT void vrEmuTms9918ScanLine(VrEmuTms9918* tms9918, uint8_t y, uint8_t pixels[TMS9918_PIXELS_X])
{
  if (tms9918 == NULL)
    return;

  if (!vrEmuTms9918DisplayEnabled(tms9918) || y >= TMS9918_PIXELS_Y)
  {
    memset(pixels, tmsMainBgColor(tms9918), TMS9918_PIXELS_X);
    return;
  }

  switch (tms9918->mode)
  {
    case TMS_MODE_GRAPHICS_I:
      vrEmuTms9918GraphicsIScanLine(tms9918, y, pixels);
      break;

    case TMS_MODE_GRAPHICS_II:
      vrEmuTms9918GraphicsIIScanLine(tms9918, y, pixels);
      break;

    case TMS_MODE_TEXT:
      vrEmuTms9918TextScanLine(tms9918, y, pixels);
      break;

    case TMS_MODE_MULTICOLOR:
      vrEmuTms9918MulticolorScanLine(tms9918, y, pixels);
      break;
  }

  if (y == TMS9918_PIXELS_Y - 1)
  {
    tms9918->status |= STATUS_INT;
  }
}

/* Function:  vrEmuTms9918RegValue
 * ----------------------------------------
 * return a reigister value
 */
VR_EMU_TMS9918_DLLEXPORT
uint8_t vrEmuTms9918RegValue(VrEmuTms9918 * tms9918, vrEmuTms9918Register reg)
{
  if (tms9918 == NULL)
    return 0;

  return tms9918->registers[reg & 0x07];
}

/* Function:  vrEmuTms9918VramValue
 * ----------------------------------------
 * return a value from vram
 */
VR_EMU_TMS9918_DLLEXPORT
uint8_t vrEmuTms9918VramValue(VrEmuTms9918* tms9918, unsigned short addr)
{
  if (tms9918 == NULL)
    return 0;

  return tms9918->vram[addr & 0x3fff];
}

/* Function:  vrEmuTms9918DisplayEnabled
  * --------------------
  * check BLANK flag
  */
VR_EMU_TMS9918_DLLEXPORT
int vrEmuTms9918DisplayEnabled(VrEmuTms9918* tms9918)
{
  if (tms9918 == NULL)
    return 0;

  return (tms9918->registers[TMS_REG_1] & 0x40) ? 1 : 0;
}
