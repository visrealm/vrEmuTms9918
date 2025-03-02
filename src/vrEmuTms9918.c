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



#include "impl/vrEmuTms9918Priv.h"

#if VR_EMU_TMS9918_SINGLE_INSTANCE

static __aligned(4) VrEmuTms9918 tms9918Inst;

VrEmuTms9918* tms9918 = &tms9918Inst;

/* Function:  vrEmuTms9918Init
 * --------------------
 * initialize the TMS9918 library in single-instance mode
 */
VR_EMU_TMS9918_DLLEXPORT
void vrEmuTms9918Init()
{
  vrEmuTms9918Reset(tms9918);
}

#else

 /* Function:  vrEmuTms9918New
  * ----------------------------------------
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

#endif


/* Function:  tmsSpriteSize
 * ----------------------------------------
 * sprite size (8 or 16)
 */
static inline uint8_t tmsSpriteSize(VrEmuTms9918* tms9918)
{
  return tms9918->registers[TMS_REG_1] & TMS_R1_SPRITE_16 ? 16 : 8;
}

/* Function:  tmsSpriteMagnification
 * ----------------------------------------
 * sprite size (0 = 1x, 1 = 2x)
 */
static inline bool tmsSpriteMag(VrEmuTms9918* tms9918)
{
  return tms9918->registers[TMS_REG_1] & TMS_R1_SPRITE_MAG2;
}

/* Function:  tmsNameTableAddr
 * ----------------------------------------
 * name table base address
 */
static inline uint16_t tmsNameTableAddr(VrEmuTms9918* tms9918)
{
  return (tms9918->registers[TMS_REG_NAME_TABLE] & 0x0f) << 10;
}

/* Function:  tmsColorTableAddr
 * ----------------------------------------
 * color table base address
 */
static inline uint16_t tmsColorTableAddr(VrEmuTms9918* tms9918)
{
  const uint8_t mask = (tms9918->mode == TMS_MODE_GRAPHICS_II) ? 0x80 : 0xff;

  return (tms9918->registers[TMS_REG_COLOR_TABLE] & mask) << 6;
}

/* Function:  tmsPatternTableAddr
 * ----------------------------------------
 * pattern table base address
 */
static inline uint16_t tmsPatternTableAddr(VrEmuTms9918* tms9918)
{
  const uint8_t mask = (tms9918->mode == TMS_MODE_GRAPHICS_II) ? 0x04 : 0x07;

  return (tms9918->registers[TMS_REG_PATTERN_TABLE] & mask) << 11;
}

/* Function:  tmsSpriteAttrTableAddr
 * ----------------------------------------
 * sprite attribute table base address
 */
static inline uint16_t tmsSpriteAttrTableAddr(VrEmuTms9918* tms9918)
{
  return (tms9918->registers[TMS_REG_SPRITE_ATTR_TABLE] & 0x7f) << 7;
}

/* Function:  tmsSpritePatternTableAddr
 * ----------------------------------------
 * sprite pattern table base address
 */
static inline uint16_t tmsSpritePatternTableAddr(VrEmuTms9918* tms9918)
{
  return (tms9918->registers[TMS_REG_SPRITE_PATT_TABLE] & 0x07) << 11;
}

/* Function:  tmsBgColor
 * ----------------------------------------
 * background color
 */
static inline vrEmuTms9918Color tmsMainBgColor(VrEmuTms9918* tms9918)
{
  return tms9918->registers[TMS_REG_FG_BG_COLOR] & 0x0f;
}

/* Function:  tmsFgColor
 * ----------------------------------------
 * foreground color
 */
static inline vrEmuTms9918Color tmsMainFgColor(VrEmuTms9918* tms9918)
{
  const vrEmuTms9918Color c = (vrEmuTms9918Color)(tms9918->registers[TMS_REG_FG_BG_COLOR] >> 4);
  return c == TMS_TRANSPARENT ? tms9918->mainBgColor : c;
}

/* Function:  tmsFgColor
 * ----------------------------------------
 * foreground color
 */
static inline vrEmuTms9918Color tmsFgColor(VrEmuTms9918* tms9918, uint8_t colorByte)
{
  const vrEmuTms9918Color c = (vrEmuTms9918Color)(colorByte >> 4);
  return c == TMS_TRANSPARENT ? tms9918->mainBgColor : c;
}

/* Function:  tmsBgColor
 * ----------------------------------------
 * background color
 */
static inline vrEmuTms9918Color tmsBgColor(VrEmuTms9918* tms9918, uint8_t colorByte)
{
  const vrEmuTms9918Color c = (vrEmuTms9918Color)(colorByte & 0x0f);
  return c == TMS_TRANSPARENT ? tms9918->mainBgColor : c;
}


/* Function:  tmsUpdateMode
 * ----------------------------------------
 * update the current display mode and cached values
 */
static void __time_critical_func(tmsUpdateMode)(VrEmuTms9918* tms9918)
{
  if (tms9918->registers[TMS_REG_0] & TMS_R0_MODE_GRAPHICS_II)
  {
    tms9918->mode = TMS_MODE_GRAPHICS_II;
  }
  else if (tms9918->registers[TMS_REG_0] & TMS_R0_MODE_TEXT_80)
  {
    tms9918->mode = TMS_MODE_TEXT80;
  }
  else
  {
    /* MC and TEX bits 3 and 4. Shift to bits 0 and 1 to determine a value (0, 1 or 2) */
    switch ((tms9918->registers[TMS_REG_1] & (TMS_R1_MODE_MULTICOLOR | TMS_R1_MODE_TEXT)) >> 3)
    {
      case 1:
        tms9918->mode = TMS_MODE_MULTICOLOR;
        break;

      case 2:
        tms9918->mode = TMS_MODE_TEXT;
        break;

      default:
        tms9918->mode = TMS_MODE_GRAPHICS_I;
        break;
    }
  }

  tms9918->displayEnabled = tms9918->registers[TMS_REG_1] & TMS_R1_DISP_ACTIVE;
  tms9918->interruptsEnabled = tms9918->registers[TMS_REG_1] & TMS_R1_INT_ENABLE;
  tms9918->spriteSize = tmsSpriteSize(tms9918);
  tms9918->spriteMag = tmsSpriteMag(tms9918);
  tms9918->patternTableAddr = tmsPatternTableAddr(tms9918);
  tms9918->colorTableAddr = tmsColorTableAddr(tms9918);
}


/* Function:  tmsMemset
 * ----------------------------------------
 * memset in 32-bit words
 */
inline void tmsMemset(uint8_t* ptr, uint8_t val8, int count)
{
  uint32_t val32 = (val8 << 8) | val8;
  val32 |= val32 << 16;

  count /= sizeof(uint32_t);

  uint32_t* ptr32 = (uint32_t*)(ptr);

  for (int i = 0; i < count; ++i)
  {
    ptr32[i] = val32;
  }
}

/* Function:  vrEmuTms9918Reset
 * ----------------------------------------
 * reset the new TMS9918
 */
VR_EMU_TMS9918_DLLEXPORT void vrEmuTms9918Reset(VR_EMU_INST_ONLY_ARG)
{
  tms9918->regWriteStage0Value = 0;
  tms9918->currentAddress = 0;
  tms9918->regWriteStage = 0;
  tms9918->status = 0;
  tms9918->readAheadBuffer = 0;
  tms9918->maxScanlineSprites = MAX_SCANLINE_SPRITES;

  tmsMemset(tms9918->registers, 0, sizeof(tms9918->registers));

  /* ram intentionally left in unknown state */

  tmsUpdateMode(tms9918);
  tms9918->nameTableAddr = tmsNameTableAddr(tms9918);
  tms9918->colorTableAddr = tmsColorTableAddr(tms9918);
  tms9918->patternTableAddr = tmsPatternTableAddr(tms9918);
  tms9918->spriteAttrTableAddr = tmsSpriteAttrTableAddr(tms9918);
  tms9918->spritePatternTableAddr = tmsSpritePatternTableAddr(tms9918);
  tms9918->mainBgColor = tmsMainBgColor(tms9918);
  tms9918->mainFgColor = tmsMainFgColor(tms9918);
}


/* Function:  vrEmuTms9918Destroy
 * ----------------------------------------
 * destroy a TMS9918
 *
 * tms9918: tms9918 object to destroy / clean up
 */
VR_EMU_TMS9918_DLLEXPORT void vrEmuTms9918Destroy(VR_EMU_INST_ONLY_ARG)
{
#if !VR_EMU_TMS9918_SINGLE_INSTANCE
  free(tms9918);
  tms9918 = NULL;
#endif
}

/* Function:  vrEmuTms9918WriteAddr
 * ----------------------------------------
 * write an address (mode = 1) to the tms9918
 *
 * data: the data (DB0 -> DB7) to send
 */
VR_EMU_TMS9918_DLLEXPORT void __time_critical_func(vrEmuTms9918WriteAddr)(VR_EMU_INST_ARG uint8_t data)
{
  vrEmuTms9918WriteAddrImpl(VR_EMU_INST data);
}

/* Function:  vrEmuTms9918ReadStatus
 * ----------------------------------------
 * read from the status register
 */
VR_EMU_TMS9918_DLLEXPORT uint8_t __time_critical_func(vrEmuTms9918ReadStatus)(VR_EMU_INST_ONLY_ARG)
{
  return vrEmuTms9918ReadStatusImpl(VR_EMU_INST_ONLY);
}

/* Function:  vrEmuTms9918PeekStatus
 * ----------------------------------------
 * read from the status register without resetting it
 */
VR_EMU_TMS9918_DLLEXPORT uint8_t __time_critical_func(vrEmuTms9918PeekStatus)(VR_EMU_INST_ONLY_ARG)
{
  return vrEmuTms9918PeekStatusImpl(VR_EMU_INST_ONLY);
}

/* Function:  vrEmuTms9918WriteData
 * ----------------------------------------
 * write data (mode = 0) to the tms9918
 *
 * data: the data (DB0 -> DB7) to send
 */
VR_EMU_TMS9918_DLLEXPORT void __time_critical_func(vrEmuTms9918WriteData)(VR_EMU_INST_ARG uint8_t data)
{
  return vrEmuTms9918WriteDataImpl(VR_EMU_INST data);
}


/* Function:  vrEmuTms9918ReadData
 * ----------------------------------------
 * read data (mode = 0) from the tms9918
 */
VR_EMU_TMS9918_DLLEXPORT uint8_t __time_critical_func(vrEmuTms9918ReadData)(VR_EMU_INST_ONLY_ARG)
{
  return vrEmuTms9918ReadDataImpl(VR_EMU_INST_ONLY);
}

/* Function:  vrEmuTms9918ReadDataNoInc
 * ----------------------------------------
 * read data (mode = 0) from the tms9918
 */
VR_EMU_TMS9918_DLLEXPORT uint8_t __time_critical_func(vrEmuTms9918ReadDataNoInc)(VR_EMU_INST_ONLY_ARG)
{
  return vrEmuTms9918ReadDataNoIncImpl(VR_EMU_INST_ONLY);
}

/* Function:  vrEmuTms9918InterruptStatus
 * --------------------
 * return true if both INT status and INT control set
 */
VR_EMU_TMS9918_DLLEXPORT bool __time_critical_func(vrEmuTms9918InterruptStatus)(VR_EMU_INST_ONLY_ARG)
{
  return vrEmuTms9918InterruptStatusImpl(VR_EMU_INST_ONLY);
}

/* Function:  vrEmuTms9918InterruptSet
 * --------------------
 * return true if both INT status and INT control set
 */
VR_EMU_TMS9918_DLLEXPORT void __time_critical_func(vrEmuTms9918InterruptSet)(VR_EMU_INST_ONLY_ARG)
{
  vrEmuTms9918InterruptSet(VR_EMU_INST_ONLY);
}

/* Function:  vrEmuTms9918SetStatus
 * --------------------
 * set status flag
 */
VR_EMU_TMS9918_DLLEXPORT
void vrEmuTms9918SetStatus(VR_EMU_INST_ARG uint8_t status)
{
  vrEmuTms9918SetStatusImpl(VR_EMU_INST status);
}

/* Function:  tmsTestCollisionMask
 * ----------------------------------------
 * Test and update the sprite collision mask.
 */
static inline uint32_t tmsTestCollisionMask(uint8_t xPos, uint32_t spritePixels, uint8_t spriteWidth)
{
  int rowSpriteBitsWord = xPos >> 5;
  int rowSpriteBitsWordBit = xPos & 0x1f;
  uint32_t validPixels = 0;

  validPixels = (~tms9918->rowSpriteBits[rowSpriteBitsWord]) & (spritePixels >> rowSpriteBitsWordBit);
  tms9918->rowSpriteBits[rowSpriteBitsWord] |= validPixels;
  validPixels <<= rowSpriteBitsWordBit;

  if ((rowSpriteBitsWordBit + spriteWidth) > 32)
  {
    uint32_t right = (~tms9918->rowSpriteBits[rowSpriteBitsWord + 1]) & (spritePixels << (32 - rowSpriteBitsWordBit));
    tms9918->rowSpriteBits[rowSpriteBitsWord + 1] |= right;
    validPixels |= (right >> (32 - rowSpriteBitsWordBit));
  }

  return validPixels;
}

/*
 * to generate the doubled pixels required when the sprite MAG flag is set,
 * use a lookup table. generate the doubledBits lookup table when we need it
 * using doubledBitsNibble.
 */
static __aligned(4) const uint8_t doubledBitsNibble[16] = {
  0x00, 0x03, 0x0c, 0x0f,
  0x30, 0x33, 0x3c, 0x3f,
  0xc0, 0xc3, 0xcc, 0xcf,
  0xf0, 0xf3, 0xfc, 0xff
};

static __aligned(4) uint16_t doubledBits[256];
static bool doubledInit = false;


/* Function:  vrEmuTms9918OutputSprites
 * ----------------------------------------
 * Output Sprites to a scanline
 */
static uint8_t __time_critical_func(vrEmuTms9918OutputSprites)(VR_EMU_INST_ARG uint8_t y, uint8_t pixels[TMS9918_PIXELS_X])
{
  const bool spriteMag = tms9918->spriteMag;
  const uint8_t spriteSize = tms9918->spriteSize;
  const bool sprite16 = spriteSize == 16;
  const uint8_t spriteIdxMask = sprite16 ? 0xfc : 0xff;
  const uint8_t spriteSizePx = spriteSize * (spriteMag + 1);
  const uint16_t spriteAttrTableAddr = tms9918->spriteAttrTableAddr;
  const uint16_t spritePatternAddr = tms9918->spritePatternTableAddr;

  uint8_t spritesShown = 0;
  uint8_t tempStatus = 0x1f;

  uint8_t* spriteAttr = tms9918->vram + spriteAttrTableAddr;
  for (uint8_t spriteIdx = 0; spriteIdx < MAX_SPRITES; ++spriteIdx)
  {
    int16_t yPos = spriteAttr[SPRITE_ATTR_Y];

    /* stop processing when yPos == LAST_SPRITE_YPOS */
    if (yPos == LAST_SPRITE_YPOS)
    {
      break;
    }

    /* check if sprite position is in the -31 to 0 range and move back to top */
    if (yPos > 0xe0)
    {
      yPos -= 256;
    }

    /* first row is YPOS -1 (0xff). 2nd row is YPOS 0 */
    yPos += 1;

    int16_t pattRow = y - yPos;
    if (spriteMag)
    {
      pattRow >>= 1;  // this needs to be a shift because -1 / 2 becomes 0. Bad.

      if (!doubledInit) // initialise our doubledBits lookup?
      {
        for (int i = 0; i < 256; ++i)
        {
          doubledBits[i] = (doubledBitsNibble[(i & 0xf0) >> 4] << 8) | doubledBitsNibble[i & 0x0f];
        }
        doubledInit = true;
      }
    }

    /* check if sprite is visible on this line */
    if (pattRow < 0 || pattRow >= spriteSize)
    {
      spriteAttr += SPRITE_ATTR_BYTES;
      continue;
    }

    if (spritesShown == 0)
    {
      for (int i = 0; i < TMS9918_PIXELS_X / 32; ++i)
      {
        tms9918->rowSpriteBits[i] = 0;
      }
    }

    /* have we exceeded the scanline sprite limit? */
    if (++spritesShown > tms9918->maxScanlineSprites)
    {
      if ((tempStatus & STATUS_5S) == 0)
      {
        tempStatus &= 0xe0;
        tempStatus |= STATUS_5S | spriteIdx;
      }
      
      break;  // allow more that 4 sprites
    }

    /* sprite is visible on this line */
    const uint8_t spriteColor = spriteAttr[SPRITE_ATTR_COLOR] & 0x0f;
    const uint8_t pattIdx = spriteAttr[SPRITE_ATTR_NAME] & spriteIdxMask;
    const uint16_t pattOffset = spritePatternAddr + pattIdx * PATTERN_BYTES + (uint16_t)pattRow;

    const int16_t earlyClockOffset = (spriteAttr[SPRITE_ATTR_COLOR] & 0x80) ? -32 : 0;
    int16_t xPos = (int16_t)(spriteAttr[SPRITE_ATTR_X]) + earlyClockOffset;

    uint8_t pattByte = tms9918->vram[pattOffset & VRAM_MASK];

    /* create a 32-bit mask of this sprite's pixels
     * left-aligned, so the first pixel in the sprite is the
     * MSB of pattMask
     */
    uint32_t pattMask = spriteMag ? doubledBits[pattByte] : pattByte;

    if (sprite16)
    {
      /* grab the next byte too */
      pattByte = tms9918->vram[(pattOffset + PATTERN_BYTES * 2) & VRAM_MASK];
      if (spriteMag)
        pattMask = (pattMask << 16) | doubledBits[pattByte];
      else
        pattMask = (pattMask << 8) | pattByte;
    }
    
    if (pattMask == 0)
    {
      spriteAttr += SPRITE_ATTR_BYTES;
      continue;
    }

    /* shift it into place (far left)*/
    pattMask <<= (32 - spriteSizePx);

    /* perform clipping operations */
    uint8_t thisSpriteSize = spriteSizePx;
    if (xPos < 0)
    {
      pattMask <<= -xPos;
      thisSpriteSize -= -xPos;
      xPos = 0;
    }
    int overflowPx = (xPos + thisSpriteSize) - TMS9918_PIXELS_X;
    if (overflowPx > 0)
    {
      thisSpriteSize -= overflowPx;
    }

    /* test and update the collision mask */
    uint32_t validPixels = tmsTestCollisionMask(xPos, pattMask, thisSpriteSize);

    /* if the result is different, we collided */
    if (validPixels != pattMask)
    {
      tempStatus |= STATUS_COL;
    }

    // Render valid pixels to the scanline
    if (spriteColor != TMS_TRANSPARENT)
    {
      while (validPixels)
      {
        if (validPixels & 0x80000000)
        {
          pixels[xPos] = spriteColor;
        }
        validPixels <<= 1;
        ++xPos;
      }
    }

    spriteAttr += SPRITE_ATTR_BYTES;
  }

  return tempStatus;
}

/* Function:  vrEmuTms9918TextScanLine
 * ----------------------------------------
 * generate a Text mode scanline
 */
static void __time_critical_func(vrEmuTms9918TextScanLine)(VR_EMU_INST_ARG uint8_t y, uint8_t pixels[TMS9918_PIXELS_X])
{
  const uint8_t tileY = y >> 3;   /* which name table row (0 - 23) */
  const uint8_t pattRow = y & 0x07;  /* which pattern row (0 - 7) */

  /* address in name table at the start of this row */
  uint8_t* rowNamesTable = tms9918->vram + tms9918->nameTableAddr + tileY * TEXT_NUM_COLS;
  const uint8_t* patternTable = tms9918->vram + tms9918->patternTableAddr + pattRow;

  const vrEmuTms9918Color bgFgColor[2] = {
    tms9918->mainBgColor,
    tms9918->mainFgColor
  };

  uint8_t* pixPtr = pixels;

  /* fill the first and last 8 pixels with bg color */
  tmsMemset(pixPtr, bgFgColor[0], TEXT_PADDING_PX);

  pixPtr += TEXT_PADDING_PX;

  for (uint8_t tileX = 0; tileX < TEXT_NUM_COLS; ++tileX)
  {
    const uint8_t pattByte = patternTable[*rowNamesTable++ * PATTERN_BYTES];

    for (uint8_t pattBit = 7; pattBit > 1; --pattBit)
    {
      *pixPtr++ = bgFgColor[(pattByte >> pattBit) & 0x01];
    }
  }
  /* fill the last 8 pixels with bg color */
  tmsMemset(pixPtr, bgFgColor[0], TEXT_PADDING_PX);
}

/* Function:  vrEmuTms9918Text80ScanLine
 * ----------------------------------------
 * generate an 80-column text mode scanline
 */
static void __time_critical_func(vrEmuTms9918Text80ScanLine)(VR_EMU_INST_ARG uint8_t y, uint8_t pixels[TMS9918_PIXELS_X])
{
  const uint8_t tileY = y >> 3;   /* which name table row (0 - 23) */
  const uint8_t pattRow = y & 0x07;  /* which pattern row (0 - 7) */

  /* address in name table at the start of this row */

  // Register 0x0A for text80 name table

  uint8_t* rowNamesTable = tms9918->vram + (tmsNameTableAddr(tms9918) & (0x0c << 10)) + tileY * TEXT80_NUM_COLS;
  const uint8_t* patternTable = tms9918->vram + tms9918->patternTableAddr + pattRow;

  const vrEmuTms9918Color bgColor = tmsMainBgColor(tms9918);
  const vrEmuTms9918Color fgColor = tmsMainFgColor(tms9918);

  const uint8_t bgFgColor[4] =
  {
    (bgColor << 4) | bgColor,
    (bgColor << 4) | fgColor,
    (fgColor << 4) | bgColor,
    (fgColor << 4) | fgColor
  };

  uint8_t* pixPtr = pixels;

  /* fill the first and last 16 pixels with bg color */
  tmsMemset(pixPtr, bgFgColor[0], TEXT_PADDING_PX);

  pixPtr += TEXT_PADDING_PX;

  for (uint8_t tileX = 0; tileX < TEXT80_NUM_COLS; ++tileX)
  {
    uint8_t pattByte = patternTable[*rowNamesTable++ * PATTERN_BYTES];
    for (uint8_t pattBit = 6; pattBit > 1; pattBit -= 2)
    {
      *pixPtr++ = bgFgColor[(pattByte >> pattBit) & 0x03];
    }
  }

  tmsMemset(pixPtr, bgFgColor[0], TEXT_PADDING_PX);
}

/* Function:  vrEmuTms9918GraphicsIScanLine
 * ----------------------------------------
 * generate a Graphics I mode scanline
 */
static void __time_critical_func(vrEmuTms9918GraphicsIScanLine)(VR_EMU_INST_ARG uint8_t y, uint8_t pixels[TMS9918_PIXELS_X])
{
  const uint8_t tileY = y >> 3;   /* which name table row (0 - 23) */
  const uint8_t pattRow = y & 0x07;  /* which pattern row (0 - 7) */

  /* address in name table at the start of this row */
  const uint16_t rowNamesAddr = tms9918->nameTableAddr + tileY * GRAPHICS_NUM_COLS;

  const uint8_t* patternTable = tms9918->vram + tms9918->patternTableAddr + pattRow;
  const uint8_t* colorTable = tms9918->vram + tms9918->colorTableAddr;

  /* iterate over each tile in this row */
  for (uint8_t tileX = 0; tileX < GRAPHICS_NUM_COLS; ++tileX)
  {
    const uint8_t pattIdx = tms9918->vram[rowNamesAddr + tileX];
    int8_t pattByte = patternTable[pattIdx * PATTERN_BYTES];
    const uint8_t colorByte = colorTable[pattIdx >> 3];

    const uint8_t bgFgColor[] = {
      tmsBgColor(tms9918, colorByte),
      tmsFgColor(tms9918, colorByte)
    };

    /* iterate over each bit of this pattern byte */
    for (uint8_t pattBit = 0; pattBit < GRAPHICS_CHAR_WIDTH; ++pattBit)
    {
      *(pixels++) = bgFgColor[pattByte < 0];
      pattByte <<= 1;
    }
  }
}

/* Function:  vrEmuTms9918GraphicsIIScanLine
 * ----------------------------------------
 * generate a Graphics II mode scanline
 */
static  __attribute__((noinline))  void __time_critical_func(vrEmuTms9918GraphicsIIScanLine)(VR_EMU_INST_ARG uint8_t y, uint8_t pixels[TMS9918_PIXELS_X])
{
  const uint8_t tileY = y >> 3;   /* which name table row (0 - 23) */
  const uint8_t pattRow = y & 0x07;  /* which pattern row (0 - 7) */

  /* address in name table at the start of this row */
  const uint16_t rowNamesAddr = tms9918->nameTableAddr + tileY * GRAPHICS_NUM_COLS;

  /* the datasheet says the lower bits of the color and pattern tables must
     be all 1's for graphics II mode. however, the lowest 2 bits of the
     pattern address are used to determine if pages 2 & 3 come from page 0
     or not. Similarly, the lowest 6 bits of the color table register are
     used as an and mask with the nametable  index */
  const uint8_t nameMask = ((tms9918->registers[TMS_REG_COLOR_TABLE] & 0x7f) << 3) | 0x07;

  const uint16_t pageThird = ((tileY & 0x18) >> 3)
    & (tms9918->registers[TMS_REG_PATTERN_TABLE] & 0x03); /* which page? 0-2 */
  const uint16_t pageOffset = pageThird << 11; /* offset (0, 0x800 or 0x1000) */

  const uint8_t* patternTable = tms9918->vram + tms9918->patternTableAddr + pageOffset + pattRow;
  const uint8_t* colorTable = tms9918->vram + tms9918->colorTableAddr + (pageOffset
    & ((tms9918->registers[TMS_REG_COLOR_TABLE] & 0x60) << 6)) + pattRow;


  /* iterate over each tile in this row */
  for (uint8_t tileX = 0; tileX < GRAPHICS_NUM_COLS; ++tileX)
  {
    uint8_t pattIdx = tms9918->vram[rowNamesAddr + tileX] & nameMask;

    const size_t pattRowOffset = pattIdx * PATTERN_BYTES;
    int8_t pattByte = patternTable[pattRowOffset];
    const uint8_t colorByte = colorTable[pattRowOffset];

    const uint8_t bgFgColor[] = {
      tmsBgColor(tms9918, colorByte),
      tmsFgColor(tms9918, colorByte)
    };

    /* iterate over each bit of this pattern byte */
    for (uint8_t pattBit = 0; pattBit < GRAPHICS_CHAR_WIDTH; ++pattBit)
    {
      *(pixels++) = bgFgColor[pattByte < 0];
      pattByte <<= 1;
    }
  }
}

/* Function:  vrEmuTms9918MulticolorScanLine
 * ----------------------------------------
 * generate a Multicolor mode scanline
 */
static void __time_critical_func(vrEmuTms9918MulticolorScanLine)(VR_EMU_INST_ARG uint8_t y, uint8_t pixels[TMS9918_PIXELS_X])
{
  const uint8_t tileY = y >> 3;
  const uint8_t pattRow = ((y >> 2) & 0x01) + (tileY & 0x03) * 2;

  const uint8_t* nameTable = tms9918->vram + tms9918->nameTableAddr + tileY * GRAPHICS_NUM_COLS;
  const uint8_t* patternTable = tms9918->vram + tms9918->patternTableAddr + pattRow;


  for (uint8_t tileX = 0; tileX < GRAPHICS_NUM_COLS; ++tileX)
  {
    const uint8_t colorByte = patternTable[nameTable[tileX] * PATTERN_BYTES];
    const uint8_t fgColor = tmsFgColor(tms9918, colorByte);
    const uint8_t bgColor = tmsBgColor(tms9918, colorByte);

    *pixels++ = fgColor;
    *pixels++ = fgColor;
    *pixels++ = fgColor;
    *pixels++ = fgColor;
    *pixels++ = bgColor;
    *pixels++ = bgColor;
    *pixels++ = bgColor;
    *pixels++ = bgColor;
  }
}

/* Function:  vrEmuTms9918ScanLine
 * ----------------------------------------
 * generate a scanline
 */
VR_EMU_TMS9918_DLLEXPORT uint8_t __time_critical_func(vrEmuTms9918ScanLine)(VR_EMU_INST_ARG uint8_t y, uint8_t pixels[TMS9918_PIXELS_X])
{
  uint8_t tempStatus = 0;

  if (!tms9918->displayEnabled || y >= TMS9918_PIXELS_Y)
  {
    tmsMemset(pixels, tmsMainBgColor(tms9918), TMS9918_PIXELS_X);
  }
  else
  {
    switch (tms9918->mode)
    {
      case TMS_MODE_GRAPHICS_I:
        vrEmuTms9918GraphicsIScanLine(VR_EMU_INST y, pixels);
        tempStatus = vrEmuTms9918OutputSprites(VR_EMU_INST y, pixels);
        break;

      case TMS_MODE_GRAPHICS_II:
        vrEmuTms9918GraphicsIIScanLine(VR_EMU_INST y, pixels);
        tempStatus = vrEmuTms9918OutputSprites(VR_EMU_INST y, pixels);
        break;

      case TMS_MODE_TEXT:
        vrEmuTms9918TextScanLine(VR_EMU_INST y, pixels);
        break;

      case TMS_MODE_MULTICOLOR:
        vrEmuTms9918MulticolorScanLine(VR_EMU_INST y, pixels);
        tempStatus = vrEmuTms9918OutputSprites(VR_EMU_INST y, pixels);
        break;

      case TMS_R0_MODE_TEXT_80:
        vrEmuTms9918Text80ScanLine(VR_EMU_INST y, pixels);
        break;
    }
  }
  return tempStatus;
}

/* Function:  vrEmuTms9918RegValue
 * ----------------------------------------
 * return a reigister value
 */
VR_EMU_TMS9918_DLLEXPORT
uint8_t __time_critical_func(vrEmuTms9918RegValue)(VR_EMU_INST_ARG vrEmuTms9918Register reg)
{
  return tms9918->registers[reg & 0x07];
}

/* Function:  vrEmuTms9918WriteRegValue
 * ----------------------------------------
 * write a reigister value
 */
VR_EMU_TMS9918_DLLEXPORT
void vrEmuTms9918WriteRegValue(VR_EMU_INST_ARG vrEmuTms9918Register reg, uint8_t value)
{
  int regIndex = reg & 0x07;
  tms9918->registers[regIndex] = value;
  switch (regIndex)
  {
    case TMS_REG_0:
    case TMS_REG_1:
      tmsUpdateMode(tms9918);
      break;

    case TMS_REG_NAME_TABLE:
      tms9918->nameTableAddr = tmsNameTableAddr(tms9918);
      break;

    case TMS_REG_COLOR_TABLE:
      tms9918->colorTableAddr = tmsColorTableAddr(tms9918);
      break;

    case TMS_REG_PATTERN_TABLE:
      tms9918->patternTableAddr = tmsPatternTableAddr(tms9918);
      break;

    case TMS_REG_SPRITE_ATTR_TABLE:
      tms9918->spriteAttrTableAddr = tmsSpriteAttrTableAddr(tms9918);
      break;

    case TMS_REG_SPRITE_PATT_TABLE:
      tms9918->spritePatternTableAddr = tmsSpritePatternTableAddr(tms9918);
      break;

    case TMS_REG_FG_BG_COLOR:
      tms9918->mainBgColor = tmsMainBgColor(tms9918);
      tms9918->mainFgColor = tmsMainFgColor(tms9918);
      break;
  }
}



/* Function:  vrEmuTms9918VramValue
 * ----------------------------------------
 * return a value from vram
 */
VR_EMU_TMS9918_DLLEXPORT
uint8_t vrEmuTms9918VramValue(VR_EMU_INST_ARG uint16_t addr)
{
  return tms9918->vram[addr & VRAM_MASK];
}

/* Function:  vrEmuTms9918DisplayEnabled
  * ----------------------------------------
  * check BLANK flag
  */
VR_EMU_TMS9918_DLLEXPORT
bool vrEmuTms9918DisplayEnabled(VR_EMU_INST_ONLY_ARG)
{
  return tms9918->displayEnabled;
}

/* Function:  vrEmuTms9918DisplayMode
  * --------------------
  * current display mode
  */
VR_EMU_TMS9918_DLLEXPORT
vrEmuTms9918Mode vrEmuTms9918DisplayMode(VR_EMU_INST_ONLY_ARG)
{
  return tms9918->mode;
}
