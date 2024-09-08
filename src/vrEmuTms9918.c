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
void __time_critical_func(vrEmuTms9918Init)()
{
  vrEmuTms9918Reset(tms9918);
}

#else

#include <stdlib.h>

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


vrEmuTms9918Mode r1Modes [] = { TMS_MODE_GRAPHICS_I, TMS_MODE_MULTICOLOR, TMS_MODE_TEXT, TMS_MODE_GRAPHICS_I };

static inline vrEmuTms9918Mode tmsMode(VrEmuTms9918* tms9918)
{
  if (tms9918->registers[TMS_REG_0] & TMS_R0_MODE_GRAPHICS_II)
    return TMS_MODE_GRAPHICS_II;
  else if (tms9918->registers[TMS_REG_0] & TMS_R0_MODE_TEXT_80)
    return TMS_MODE_TEXT80;
  else
    return r1Modes [(tms9918->registers[TMS_REG_1] & (TMS_R1_MODE_MULTICOLOR | TMS_R1_MODE_TEXT)) >> 3];
}

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

/* Function:  tmsNameTable2Addr
 * ----------------------------------------
 * name table base address
 */
static inline uint16_t tmsNameTable2Addr(VrEmuTms9918* tms9918)
{
  return (tms9918->registers[10] & 0x0f) << 10;
}

/* Function:  tmsColorTableAddr
 * ----------------------------------------
 * color table base address
 */
static inline uint16_t tmsColorTableAddr(VrEmuTms9918* tms9918)
{
  const uint8_t mask = (tmsMode(tms9918) == TMS_MODE_GRAPHICS_II) ? 0x80 : 0xff;

  return (tms9918->registers[TMS_REG_COLOR_TABLE] & mask) << 6;
}

/* Function:  tmsColorTable2Addr
 * ----------------------------------------
 * color table base address
 */
static inline uint16_t tmsColorTable2Addr(VrEmuTms9918* tms9918)
{
  const uint8_t mask = (tmsMode(tms9918) == TMS_MODE_GRAPHICS_II) ? 0x80 : 0xff;

  return (tms9918->registers[11] & mask) << 6;
}

/* Function:  tmsPatternTableAddr
 * ----------------------------------------
 * pattern table base address
 */
static inline uint16_t tmsPatternTableAddr(VrEmuTms9918* tms9918)
{
  const uint8_t mask = (tmsMode(tms9918) == TMS_MODE_GRAPHICS_II) ? 0x04 : 0x07;

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
  return c == TMS_TRANSPARENT ? tmsMainBgColor(tms9918) : c;
}

/* Function:  tmsFgColor
 * ----------------------------------------
 * foreground color
 */
static inline vrEmuTms9918Color tmsFgColor(VrEmuTms9918* tms9918, uint8_t colorByte)
{
  const vrEmuTms9918Color c = (vrEmuTms9918Color)(colorByte >> 4);
  return c == TMS_TRANSPARENT ? tmsMainBgColor(tms9918) : c;
}

/* Function:  tmsBgColor
 * ----------------------------------------
 * background color
 */
static inline vrEmuTms9918Color tmsBgColor(VrEmuTms9918* tms9918, uint8_t colorByte)
{
  const vrEmuTms9918Color c = (vrEmuTms9918Color)(colorByte & 0x0f);
  return c == TMS_TRANSPARENT ? tmsMainBgColor(tms9918) : c;
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

const uint16_t defaultPalette[] = {
0x000, // -- 0 Transparent
0x000, // -- 1 Black
0x2C3, // -- 2 Medium Green
0x5D6, // -- 3 Light Green
0x54F, // -- 4 Dark Blue
0x76F, // -- 5 Light Blue
0xD54, // -- 6 Dark Red
0x4EF, // -- 7 Cyan
0xF54, // -- 8 Medium Red
0xF76, // -- 9 Light Red
0xDC3, // -- 10 Dark Yellow
0xED6, // -- 11 Light Yellow
0x2B2, // -- 12 Dark Green
0xC5C, // -- 13 Magenta
0xCCC, // -- 14 Gray
0xFFF, // -- 15 White
//-- Palette 1, ECM1 (0 index is always 000) version of palette 0
0x000, // -- 0 Black
0x2C3, // -- 1 Medium Green
0x000, // -- 2 Black
0x54F, // -- 3 Dark Blue
0x000, // -- 4 Black
0xD54, // -- 5 Dark Red
0x000, // -- 6 Black
0x4EF, // -- 7 Cyan
0x000, // -- 8 Black
0xCCC, // -- 9 Gray
0x000, // -- 10 Black
0xDC3, // -- 11 Dark Yellow
0x000, // -- 12 Black
0xC5C, // -- 13 Magenta
0x000, // -- 14 Black
0xFFF, // -- 15 White
//-- Palette 2, CGA colors
0x000, // -- 0 >000000 ( 0 0 0) black
0x00A, // -- 1 >0000AA ( 0 0 170) blue
0x0A0, // -- 2 >00AA00 ( 0 170 0) green
0x0AA, // -- 3 >00AAAA ( 0 170 170) cyan
0xA00, // -- 4 >AA0000 (170 0 0) red
0xA0A, // -- 5 >AA00AA (170 0 170) magenta
0xA50, // -- 6 >AA5500 (170 85 0) brown
0xAAA, // -- 7 >AAAAAA (170 170 170) light gray
0x555, // -- 8 >555555 ( 85 85 85) gray
0x55F, // -- 9 >5555FF ( 85 85 255) light blue
0x5F5, // -- 10 >55FF55 ( 85 255 85) light green
0x5FF, // -- 11 >55FFFF ( 85 255 255) light cyan
0xF55, // -- 12 >FF5555 (255 85 85) light red
0xF5F, // -- 13 >FF55FF (255 85 255) light magenta
0xFF5, // -- 14 >FFFF55 (255 255 85) yellow
0xFFF, // -- 15 >FFFFFF (255 255 255) white
//-- Palette 3, ECM1 (0 index is always 000) version of palette 2
0x000, // -- 0 >000000 ( 0 0 0) black
0x555, // -- 1 >555555 ( 85 85 85) gray
0x000, // -- 2 >000000 ( 0 0 0) black
0x00A, // -- 3 >0000AA ( 0 0 170) blue
0x000, // -- 4 >000000 ( 0 0 0) black
0x0A0, // -- 5 >00AA00 ( 0 170 0) green
0x000, // -- 6 >000000 ( 0 0 0) black
0x0AA, // -- 7 >00AAAA ( 0 170 170) cyan
0x000, // -- 8 >000000 ( 0 0 0) black
0xA00, // -- 9 >AA0000 (170 0 0) red
0x000, // -- 10 >000000 ( 0 0 0) black
0xA0A, // -- 11 >AA00AA (170 0 170) magenta
0x000, // -- 12 >000000 ( 0 0 0) black
0xA50, // -- 13 >AA5500 (170 85 0) brown
0x000, // -- 14 >000000 ( 0 0 0) black
0xFFF  // -- 15 >FFFFFF (255 255 255) white
};

/* Function:  vrEmuTms9918Reset
 * ----------------------------------------
 * reset the new TMS9918
 */
VR_EMU_TMS9918_DLLEXPORT void __time_critical_func(vrEmuTms9918Reset)(VR_EMU_INST_ONLY_ARG)
{
  tms9918->regWriteStage0Value = 0;
  tms9918->currentAddress = 0;
  tms9918->gpuAddress = 0xFFFF; // "Odd" don't start value
  tms9918->regWriteStage = 0;
  tmsMemset(tms9918->status, 0, sizeof(tms9918->status));
  tms9918->status [1] = 0xE0;  // ID = F18A
  tms9918->status [14] = 0x1A; // Version - one more than the F18A's 1.9
  tms9918->readAheadBuffer = 0;
  tms9918->lockedMask = 0x07;
  tms9918->unlockCount = 0;
  tms9918->restart = 0;
  tmsMemset(tms9918->registers, 0, sizeof(tms9918->registers));
  tms9918->registers [0x30] = 1; // vram address increment register
  tms9918->registers [0x33] = 32; // Sprites to process

  // set up default palettes
  for (int i = 0; i < sizeof(defaultPalette) / sizeof(uint16_t); ++i)
  {
    tms9918->pram[i] = defaultPalette[i];
  }

  /* ram intentionally left in unknown state */
}


/* Function:  vrEmuTms9918Destroy
 * ----------------------------------------
 * destroy a TMS9918
 *
 * tms9918: tms9918 object to destroy / clean up
 */
VR_EMU_TMS9918_DLLEXPORT void __time_critical_func(vrEmuTms9918Destroy)(VR_EMU_INST_ONLY_ARG)
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
void __time_critical_func(vrEmuTms9918SetStatus)(VR_EMU_INST_ARG uint8_t status)
{
  vrEmuTms9918SetStatusImpl(VR_EMU_INST status);
}

/* Function:  tmsTestCollisionMask
 * ----------------------------------------
 * Test and update the sprite collision mask.
 */
static inline uint32_t tmsTestCollisionMask(VR_EMU_INST_ARG uint8_t xPos, uint32_t spritePixels, uint8_t spriteWidth)
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
__aligned(4) uint8_t doubledBitsNibble[16] = {
  0x00, 0x03, 0x0c, 0x0f,
  0x30, 0x33, 0x3c, 0x3f,
  0xc0, 0xc3, 0xcc, 0xcf,
  0xf0, 0xf3, 0xfc, 0xff
};


/* lookup for combining ecm nibbles, returning 4 pixels */
static uint16_t __aligned(4) ecmLookup[16 * 16 * 16];
static bool ecmLookupReady = false;

static uint8_t ecmByte(bool h, bool m, bool l)
{
  return (h << 2) | (m << 1) | l;
}

/* lookup from bit planes: 333322221111 to merged palette values for four pixels
 * NOTE: The left-most pixel is stored in the least significant nibble of the result
 *       because it's more efficient to offload them that way
 */
static void ecmLookupInit()
{
  for (uint16_t i = 0; i < 16 * 16 * 16; ++i)
  {
    ecmLookup[i] = (ecmByte(i & 0x800, i & 0x080, i & 0x008)) |
                   (ecmByte(i & 0x400, i & 0x040, i & 0x004) << 4) |
                   (ecmByte(i & 0x200, i & 0x020, i & 0x002) << 8) |
                   (ecmByte(i & 0x100, i & 0x010, i & 0x001) << 12);
  }

  ecmLookupReady = true;
}


/* lookup for doubling pixel patterns in mag mode */
static __aligned(4) uint16_t doubledBits[256];
static bool doubledBitsReady = false;

static void doubledBitsInit()
{
  for (int i = 0; i < 256; ++i)
  {
    doubledBits[i] = (doubledBitsNibble[(i & 0xf0) >> 4] << 8) | doubledBitsNibble[i & 0x0f];
  }
  doubledBitsReady = true;
}



/* Function:  vrEmuTms9918OutputSprites
 * ----------------------------------------
 * Output Sprites to a scanline
 */
static uint8_t __time_critical_func(vrEmuTms9918OutputSprites)(VR_EMU_INST_ARG uint8_t y, uint8_t pixels[TMS9918_PIXELS_X])
{
  const bool spriteMag = tmsSpriteMag(tms9918);
  const uint8_t spriteSize = tmsSpriteSize(tms9918);
  const bool sprite16 = spriteSize == 16;
  const uint8_t spriteIdxMask = sprite16 ? 0xfc : 0xff;
  const uint8_t spriteSizePx = spriteSize * (spriteMag + 1);
  const uint16_t spriteAttrTableAddr = tmsSpriteAttrTableAddr(tms9918);
  const uint16_t spritePatternAddr = tmsSpritePatternTableAddr(tms9918);

  uint8_t spritesShown = 0;
  uint8_t tempStatus = 0x1f;

  // ecm settings
  uint8_t ecm = (tms9918->registers[0x31] & 0x03);
  uint8_t ecmColorOffset = (ecm == 3) ? 2 : ecm;
  uint8_t ecmColorMask = (ecm == 3) ? 0x0e : 0x0f;
  int ecmOffset = 0x800 >> ((tms9918->registers[0x1d] & 0xc0) >> 6);

  uint8_t pal = tms9918->registers[0x18] & 0x30;
  if (ecm == 1)
  {
    pal &= 0x20;
  }
  else if (ecm)
  {
    pal = 0;
  }

  int maxSprites = tms9918->registers [0x33];
  if (maxSprites > MAX_SPRITES) maxSprites = MAX_SPRITES;


  uint8_t* spriteAttr = tms9918->vram + spriteAttrTableAddr;
  for (uint8_t spriteIdx = 0; spriteIdx < maxSprites; ++spriteIdx)
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

      if (!doubledBitsReady) // initialise our doubledBits lookup?
      {
        doubledBitsInit();
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
    if (++spritesShown > MAX_SCANLINE_SPRITES)
    {
      if ((tempStatus & STATUS_5S) == 0)
      {
        tempStatus &= 0xe0;
        tempStatus |= STATUS_5S | spriteIdx;
      }
      break;
    }

    /* sprite is visible on this line */
    uint8_t spriteColor = (spriteAttr[SPRITE_ATTR_COLOR] & ecmColorMask) << ecmColorOffset;
    const uint8_t pattIdx = spriteAttr[SPRITE_ATTR_NAME] & spriteIdxMask;
    uint16_t pattOffset = spritePatternAddr + pattIdx * PATTERN_BYTES + (uint16_t)pattRow;

    const int16_t earlyClockOffset = (spriteAttr[SPRITE_ATTR_COLOR] & 0x80) ? -32 : 0;
    int16_t xPos = (int16_t)(spriteAttr[SPRITE_ATTR_X]) + earlyClockOffset;

    /* create a 32-bit mask of this sprite's pixels
     * left-aligned, so the first pixel in the sprite is the
     * MSB of spriteBits
     */
    bool haveBits = false;
    uint16_t spriteBits[3] = {0}; // a 16-bit value for each ecm bit plane
    for (int i = 0; !i || (i < ecm); ++i)
    {
      spriteBits[i] = ((uint16_t)tms9918->vram[pattOffset]) << 8;
      if (sprite16)
      {
        spriteBits[i] |= ((uint16_t)tms9918->vram[(pattOffset + PATTERN_BYTES * 2)]);
      }
      pattOffset += ecmOffset;
      if (spriteBits[i]) haveBits = true;
    }


    /* exit early if no bits to draw */
    if (!haveBits)
    {
      spriteAttr += SPRITE_ATTR_BYTES;
      continue;
    }

    /* perform clipping operations */
    uint8_t thisSpriteSize = spriteSize;
    if (xPos < 0)
    {
      for (int i = 0; i == 0 || i < ecm; ++i)
      {
        spriteBits[i] <<= -xPos;
      }
      thisSpriteSize -= -xPos;
      xPos = 0;
    }
    int overflowPx = (xPos + thisSpriteSize) - TMS9918_PIXELS_X;
    if (overflowPx > 0)
    {
      thisSpriteSize -= overflowPx;
    }

    uint32_t pattMask = 0;
    
    if (ecm)
    {
      int mask = 0x8000;
      for (int x = 0; x < spriteSize; ++x)
      {
        for (int i = 0; i < ecm; ++i)
        {
          if (spriteBits[i] & mask)
          {
            pattMask |= mask;
            break;
          }
        }
        mask >>= 1;
      }
    }
    else
    {
      pattMask = spriteBits[0];
    }

    /* do something about magnified sprites */
    pattMask <<= 16;

    /* test and update the collision mask */
    uint32_t validPixels = tmsTestCollisionMask(VR_EMU_INST xPos, pattMask, thisSpriteSize);

    /* if the result is different, we collided */
    if (validPixels != pattMask)
    {
      tempStatus |= STATUS_COL;
    }

    // Render valid pixels to the scanline
    if (spriteColor != TMS_TRANSPARENT)
    {
      spriteColor |= pal;
      if (ecm)
      {
        while (validPixels)
        {
          if (validPixels & 0xf0000000)
          {
            uint16_t color = ecmLookup[((spriteBits[0] & 0xf000) >> 12) | ((spriteBits[1] & 0xf000) >> 8) | ((spriteBits[2] & 0xf000) >> 4)];

            for (int i = 0; i < 4; ++i)
            {
              uint8_t pix = color & 0x7;
              if (pix) pixels[xPos + i] = spriteColor | pix;
              color >>= 4;
            }

          }
          validPixels <<= 4;
          spriteBits[0] <<= 4;
          spriteBits[1] <<= 4;
          spriteBits[2] <<= 4;
          xPos += 4;
        }
      }
      else
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
    }

    spriteAttr += SPRITE_ATTR_BYTES;
  }

  return tempStatus;
}

static uint8_t __time_critical_func(vrEmuTms9918OutputSprites2)(VR_EMU_INST_ARG uint8_t y)
{
  const bool spriteMag = tmsSpriteMag(tms9918);
  const uint8_t spriteSize = tmsSpriteSize(tms9918);
  const uint16_t spriteAttrTableAddr = tmsSpriteAttrTableAddr(tms9918);

  uint8_t spritesShown = 0;
  uint8_t tempStatus = 0;

  uint8_t* spriteAttr = tms9918->vram + spriteAttrTableAddr;
  for (uint8_t spriteIdx = 0; spriteIdx < (MAX_SPRITES < tms9918->registers [0x33] ? MAX_SPRITES : tms9918->registers [0x33]); ++spriteIdx)
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
    }

    /* check if sprite is visible on this line */
    if (pattRow < 0 || pattRow >= spriteSize)
    {
      spriteAttr += SPRITE_ATTR_BYTES;
      continue;
    }

    /* have we exceeded the scanline sprite limit? */
    if (++spritesShown > MAX_SCANLINE_SPRITES)
    {
      tempStatus = STATUS_5S | spriteIdx;
      break;
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
  uint8_t* rowNamesTable = tms9918->vram + tmsNameTableAddr(tms9918) + tileY * TEXT_NUM_COLS;
  const uint8_t* patternTable = tms9918->vram + tmsPatternTableAddr(tms9918) + pattRow;

  const vrEmuTms9918Color bgFgColor[2] = {
    tmsMainBgColor(tms9918),
    tmsMainFgColor(tms9918)
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

  uint8_t* rowNamesTable = tms9918->vram /*+ tmsNameTableAddr(tms9918)*/ + tileY * TEXT80_NUM_COLS;
  const uint8_t* patternTable = tms9918->vram + tmsPatternTableAddr(tms9918) + pattRow;

  const vrEmuTms9918Color bgColor = TMS_DK_BLUE;//tmsMainBgColor(tms9918);
  const vrEmuTms9918Color fgColor = TMS_WHITE;//tmsMainFgColor(tms9918);

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

static void __time_critical_func(vrEmuF18AT1ScanLine)(VR_EMU_INST_ARG uint8_t y, uint8_t pixels[TMS9918_PIXELS_X])
{
  uint8_t *end = pixels + TMS9918_PIXELS_X;
  bool swapYPage = false;

  if (tms9918->registers[0x1c])
  {
    int virtY = y;
    virtY += tms9918->registers[0x1c];

    int maxY = (tms9918->registers[0x31] & 0x40) ? (8 * 30) : (8 * 24);

    if (virtY >= maxY)
    {
      virtY -= maxY;

      if (tms9918->registers[0x1d] & 0x01) // multiple vertical pages?
      {
        swapYPage = true;
      }
    }
   
    y = virtY;
  }

  uint8_t ecm = (tms9918->registers[0x31] & 0x30) >> 4;
  uint8_t ecmColorOffset = (ecm == 3) ? 2 : ecm;
  uint8_t ecmColorMask = (ecm == 3) ? 0x0e : 0x0f;
  int ecmOffset = 0x800 >> ((tms9918->registers[0x1d] & 0x0c) >> 2);

  uint8_t pal = (tms9918->registers[0x18] & 0x03) << 4;
  if (ecm == 1)
  {
    pal &= 0x20;
  }
  else if (ecm)
  {
    pal = 0;
  }

  const uint8_t tileY = y >> 3;   /* which name table row (0 - 23)... or 29 */
  const uint8_t pattRow = y & 0x07;  /* which pattern row (0 - 7) */

  /* address in name table at the start of this row */
  uint16_t rowNamesAddr = tmsNameTableAddr(tms9918) + tileY * GRAPHICS_NUM_COLS;
  if (swapYPage) rowNamesAddr ^= 0x800;

  const uint8_t* patternTable = tms9918->vram + tmsPatternTableAddr(tms9918);
  const uint8_t* colorTable = tms9918->vram + tmsColorTableAddr(tms9918);

  uint8_t startPattBit = tms9918->registers[0x1B] & 0x07;
  uint8_t tileIndex = tms9918->registers[0x1B] >> 3;

  /* iterate over each tile in this row */
  while (pixels < end)
  {
    if (tileIndex == GRAPHICS_NUM_COLS)
    {
      if (tms9918->registers[0x1d] & 0x02) rowNamesAddr ^= 0x400;
      tileIndex = 0;
    }

    const uint8_t pattIdx = tms9918->vram[rowNamesAddr + tileIndex];
    uint16_t pattOffset = pattIdx * PATTERN_BYTES;

    if (ecm)
    {
    /* iterate over each bit of this pattern byte */
      const uint8_t colorByte = colorTable[pattIdx];
      const bool flipX = 0;//colorByte & 0x40;
      const bool flipY = 0;//colorByte & 0x20;
      int16_t pattBytesHigh;
      int16_t pattBytesLow;

      pattOffset += pattRow;//flipY ? 7 - pattRow : pattRow;

      uint8_t ecmColor = pal | ((colorByte & ecmColorMask) << ecmColorOffset);

/*      if (flipX)
      {
        
        for (int i = 0; i < ecm; ++i)
        {
          pattByte[i] = patternTable[pattOffset] >> startPattBit;
          pattOffset += ecmOffset;
        }
        
        for (uint8_t pattBit = startPattBit; pattBit < GRAPHICS_CHAR_WIDTH; ++pattBit)
        {
          uint8_t pixColor = ecmColor;
          for (int i = 0; i < ecm; ++i)
          {
            pixColor |= (pattByte[i] & 0x01) << i;
            pattByte[i] >>= 1;
          }
          
          *(pixels++) = pixColor;
        }
      }
      else*/
      {
        uint8_t value = patternTable[pattOffset] << startPattBit;
        pattBytesHigh = (value & 0xf0) >> 4;
        pattBytesLow = (value & 0xf);

        for (int i = 1; i < ecm; ++i)
        {
          pattOffset += ecmOffset;
          value = patternTable[pattOffset] << startPattBit;
          pattBytesLow |= (value & 0xf) << ((i) * 4); value>>=4;
          pattBytesHigh |= (value & 0xf) << ((i) * 4);
        }

        uint32_t pattern = ((uint32_t)ecmLookup[pattBytesLow] << 16) | ecmLookup[pattBytesHigh];

        int count = 8 - startPattBit;
        if (count > (end - pixels)) count = end - pixels;

        while (count--)
        {
          uint8_t pixColor = (pattern & 0x07);
          *pixels = ecmColor | pixColor;
          pattern >>= 4;
          ++pixels;
        }
      }
    }
    else
    {
      int8_t pattByte = patternTable[pattOffset + pattRow] << startPattBit;
      const uint8_t colorByte = colorTable[pattIdx >> 3];

      const uint8_t bgFgColor[] = {
        pal | tmsBgColor(tms9918, colorByte),
        pal | tmsFgColor(tms9918, colorByte)
      };

      for (uint8_t pattBit = startPattBit; (pattBit < GRAPHICS_CHAR_WIDTH) && (pixels < end); ++pattBit)
      {
        *(pixels++) = bgFgColor[pattByte < 0];
        pattByte <<= 1;
      }
    }
    startPattBit = 0;
    ++tileIndex;
  }
}


static void __time_critical_func(vrEmuF18AT2ScanLine)(VR_EMU_INST_ARG uint8_t y, uint8_t pixels[TMS9918_PIXELS_X])
{
  uint8_t *end = pixels + TMS9918_PIXELS_X;
  bool swapYPage = false;

  if (tms9918->registers[0x1a])
  {
    int virtY = y;
    virtY += tms9918->registers[0x1a];

    int maxY = (tms9918->registers[0x31] & 0x40) ? (8 * 30) : (8 * 24);

    if (virtY >= maxY)
    {
      virtY -= maxY;

      if (tms9918->registers[0x1d] & 0x10) // multiple vertical pages?
      {
        swapYPage = true;
      }
    }
   
    y = virtY;
  }

  uint8_t ecm = (tms9918->registers[0x31] & 0x30) >> 4;
  uint8_t ecmColorOffset = (ecm == 3) ? 2 : ecm;
  uint8_t ecmColorMask = (ecm == 3) ? 0x0e : 0x0f;
  int ecmOffset = 0x800 >> ((tms9918->registers[0x1d] & 0xc0) >> 6);

  uint8_t pal = (tms9918->registers[0x18] & 0x0c) << 2;
  if (ecm == 1)
  {
    pal &= 0x20;
  }
  else if (ecm)
  {
    pal = 0;
  }

  const uint8_t tileY = y >> 3;   /* which name table row (0 - 23)... or 29 */
  const uint8_t pattRow = y & 0x07;  /* which pattern row (0 - 7) */

  /* address in name table at the start of this row */
  uint16_t rowNamesAddr = tmsNameTable2Addr(tms9918) + tileY * GRAPHICS_NUM_COLS;
  if (swapYPage) rowNamesAddr ^= 0x800;

  const uint8_t* patternTable = tms9918->vram + tmsPatternTableAddr(tms9918);
  const uint8_t* colorTable = tms9918->vram + tmsColorTable2Addr(tms9918);

  uint8_t startPattBit = tms9918->registers[0x19] & 0x07;
  uint8_t tileIndex = tms9918->registers[0x19] >> 3;

  /* iterate over each tile in this row */
  while (pixels < end)
  {
    if (tileIndex == GRAPHICS_NUM_COLS)
    {
      if (tms9918->registers[0x1d] & 0x20) rowNamesAddr ^= 0x400;
      tileIndex = 0;
    }

    const uint8_t pattIdx = tms9918->vram[rowNamesAddr + tileIndex];
    uint16_t pattOffset = pattIdx * PATTERN_BYTES;

    if (ecm)
    {
    /* iterate over each bit of this pattern byte */
      const uint8_t colorByte = colorTable[pattIdx];
      const bool flipX = 0;//colorByte & 0x40;
      const bool flipY = 0;//colorByte & 0x20;
      int16_t pattBytesHigh;
      int16_t pattBytesLow;

      pattOffset += pattRow;//flipY ? 7 - pattRow : pattRow;

      uint8_t ecmColor = pal | ((colorByte & ecmColorMask) << ecmColorOffset);

/*      if (flipX)
      {
        
        for (int i = 0; i < ecm; ++i)
        {
          pattByte[i] = patternTable[pattOffset] >> startPattBit;
          pattOffset += ecmOffset;
        }
        
        for (uint8_t pattBit = startPattBit; pattBit < GRAPHICS_CHAR_WIDTH; ++pattBit)
        {
          uint8_t pixColor = ecmColor;
          for (int i = 0; i < ecm; ++i)
          {
            pixColor |= (pattByte[i] & 0x01) << i;
            pattByte[i] >>= 1;
          }
          
          *(pixels++) = pixColor;
        }
      }
      else*/
      {
        uint8_t value = patternTable[pattOffset] << startPattBit;
        pattBytesHigh = (value & 0xf0) >> 4;
        pattBytesLow = (value & 0xf);

        for (int i = 1; i < ecm; ++i)
        {
          pattOffset += ecmOffset;
          value = patternTable[pattOffset] << startPattBit;
          pattBytesLow |= (value & 0xf) << ((i) * 4); value>>=4;
          pattBytesHigh |= (value & 0xf) << ((i) * 4);
        }

        uint32_t pattern = ((uint32_t)ecmLookup[pattBytesLow] << 16) | ecmLookup[pattBytesHigh];

        int count = 8 - startPattBit;
        if (count > (end - pixels)) count = end - pixels;

        while (count--)
        {
          uint8_t pixColor = (pattern & 0x07);
          if (pixColor) *pixels = ecmColor | pixColor;
          pattern >>= 4;
          ++pixels;
        }
      }
    }
    else
    {
      int8_t pattByte = patternTable[pattOffset + pattRow] << startPattBit;
      const uint8_t colorByte = colorTable[pattIdx >> 3];

      const uint8_t bgFgColor[] = {
        pal | tmsBgColor(tms9918, colorByte),
        pal | tmsFgColor(tms9918, colorByte)
      };

      for (uint8_t pattBit = startPattBit; (pattBit < GRAPHICS_CHAR_WIDTH) && (pixels < end); ++pattBit)
      {
        *(pixels++) = bgFgColor[pattByte < 0];
        pattByte <<= 1;
      }
    }
    startPattBit = 0;
    ++tileIndex;
  }
}


/* Function:  vrEmuTms9918GraphicsIScanLine
 * ----------------------------------------
 * generate a Graphics I mode scanline
 */
static void __time_critical_func(vrEmuTms9918GraphicsIScanLine)(VR_EMU_INST_ARG uint8_t y, uint8_t pixels[TMS9918_PIXELS_X])
{
  if (!ecmLookupReady) ecmLookupInit();

  vrEmuF18AT1ScanLine(y, pixels);

  if (tms9918->registers[0x31] & 0x80) vrEmuF18AT2ScanLine(y, pixels);
  //if (tms9918->registers[0x32] & 0x10) return;  // TL1_OFF?

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
  const uint16_t rowNamesAddr = tmsNameTableAddr(tms9918) + tileY * GRAPHICS_NUM_COLS;

  /* the datasheet says the lower bits of the color and pattern tables must
     be all 1's for graphics II mode. however, the lowest 2 bits of the
     pattern address are used to determine if pages 2 & 3 come from page 0
     or not. Similarly, the lowest 6 bits of the color table register are
     used as an and mask with the nametable  index */
  const uint8_t nameMask = ((tms9918->registers[TMS_REG_COLOR_TABLE] & 0x7f) << 3) | 0x07;

  const uint16_t pageThird = ((tileY & 0x18) >> 3)
    & (tms9918->registers[TMS_REG_PATTERN_TABLE] & 0x03); /* which page? 0-2 */
  const uint16_t pageOffset = pageThird << 11; /* offset (0, 0x800 or 0x1000) */

  const uint8_t* patternTable = tms9918->vram + tmsPatternTableAddr(tms9918) + pageOffset + pattRow;
  const uint8_t* colorTable = tms9918->vram + tmsColorTableAddr(tms9918) + (pageOffset
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

  const uint8_t* nameTable = tms9918->vram + tmsNameTableAddr(tms9918) + tileY * GRAPHICS_NUM_COLS;
  const uint8_t* patternTable = tms9918->vram + tmsPatternTableAddr(tms9918) + pattRow;


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
  uint8_t tempStatus2 = 0;

  if (!(tms9918->registers[TMS_REG_1] & TMS_R1_DISP_ACTIVE))// || y >= TMS9918_PIXELS_Y)
  {
    tmsMemset(pixels, tmsMainBgColor(tms9918), TMS9918_PIXELS_X);
  }
  else
  {
    switch (tmsMode(tms9918))
    {
      case TMS_MODE_GRAPHICS_I:
        vrEmuTms9918GraphicsIScanLine(VR_EMU_INST y, pixels);
        tempStatus = vrEmuTms9918OutputSprites(VR_EMU_INST y, pixels);
        /*if ((tempStatus & STATUS_5S) == 0) {
            tempStatus2 = vrEmuTms9918OutputSprites2(VR_EMU_INST y + 1);
            if (tempStatus2 & STATUS_5S)
                tempStatus = (tempStatus & 0xe0) | tempStatus2;
        }*/
        break;

      case TMS_MODE_GRAPHICS_II:
        vrEmuTms9918GraphicsIIScanLine(VR_EMU_INST y, pixels);
        tempStatus = vrEmuTms9918OutputSprites(VR_EMU_INST y, pixels);
        /*if ((tempStatus & STATUS_5S) == 0) {
            tempStatus2 = vrEmuTms9918OutputSprites2(VR_EMU_INST y + 1);
            if (tempStatus2 & STATUS_5S)
                tempStatus = (tempStatus & 0xe0) | tempStatus2;
        }*/
        break;

      case TMS_MODE_TEXT:
        vrEmuTms9918TextScanLine(VR_EMU_INST y, pixels);
        break;

      case TMS_MODE_MULTICOLOR:
        vrEmuTms9918MulticolorScanLine(VR_EMU_INST y, pixels);
        tempStatus = vrEmuTms9918OutputSprites(VR_EMU_INST y, pixels);
        /*if ((tempStatus & STATUS_5S) == 0) {
            tempStatus2 = vrEmuTms9918OutputSprites2(VR_EMU_INST y + 1);
            if (tempStatus2 & STATUS_5S)
                tempStatus = (tempStatus & 0xe0) | tempStatus2;
        }*/
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
  return tms9918->registers[reg & tms9918->lockedMask]; // was 0x07
}

/* Function:  vrEmuTms9918WriteRegValue
 * ----------------------------------------
 * write a reigister value
 */
VR_EMU_TMS9918_DLLEXPORT
void __time_critical_func(vrEmuTms9918WriteRegValue)(VR_EMU_INST_ARG vrEmuTms9918Register reg, uint8_t value)
{
  if ((reg == (0x80 | 0x39)) && (value == 0x1C)) {
    tms9918->registers[0x39] = 0x1C; // Allow this one through even when locked
    if (++tms9918->unlockCount == 2)
    {
      tms9918->unlockCount = 0;
      tms9918->lockedMask = 0x3f;
    }
  } else {
    tms9918->unlockCount = 0;
    int regIndex = reg & tms9918->lockedMask; // was 0x07
    tms9918->registers[regIndex] = value;
    if ((regIndex == 0x37) || ((regIndex == 0x38) && ((value & 1) == 0))) {
      tms9918->gpuAddress = ((tms9918->registers [0x36] << 8) | tms9918->registers [0x37]) & 0xFFFE;
      if (regIndex == 0x37) {
        tms9918->registers [0x38] = 0;
        tms9918->restart = 1;
      }
    } else
    if ((regIndex == 0x38) && (value & 1)) {
      tms9918->restart = 1;
    } else
    if (regIndex == 0x0F)
      tms9918->status [0x0F] = value;
  }
}



/* Function:  vrEmuTms9918VramValue
 * ----------------------------------------
 * return a value from vram
 */
VR_EMU_TMS9918_DLLEXPORT
uint8_t __time_critical_func(vrEmuTms9918VramValue)(VR_EMU_INST_ARG uint16_t addr)
{
  return tms9918->vram[addr & VRAM_MASK];
}

/* Function:  vrEmuTms9918DisplayEnabled
  * ----------------------------------------
  * check BLANK flag
  */
VR_EMU_TMS9918_DLLEXPORT
bool __time_critical_func(vrEmuTms9918DisplayEnabled)(VR_EMU_INST_ONLY_ARG)
{
  return (tms9918->registers[TMS_REG_1] & TMS_R1_DISP_ACTIVE);
}

/* Function:  vrEmuTms9918DisplayMode
  * --------------------
  * current display mode
  */
VR_EMU_TMS9918_DLLEXPORT
vrEmuTms9918Mode __time_critical_func(vrEmuTms9918DisplayMode)(VR_EMU_INST_ONLY_ARG)
{
  return tmsMode(tms9918);
}
