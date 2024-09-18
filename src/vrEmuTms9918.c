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

#include "pico/divider.h"

#if VR_EMU_TMS9918_SINGLE_INSTANCE

static VrEmuTms9918 __aligned(8) tms9918Inst;

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
  tms9918->registers [0x1e] = MAX_SCANLINE_SPRITES; // scanline sprites
  tms9918->registers [0x30] = 1; // vram address increment register
  tms9918->registers [0x33] = MAX_SPRITES; // Sprites to process

  // set up default palettes (arm is little-endian, tms9900 is big-endian)
  for (int i = 0; i < sizeof(defaultPalette) / sizeof(uint16_t); ++i)
  {
    tms9918->pram[i] = __builtin_bswap16(defaultPalette[i]);
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

static __attribute__((section(".scratch_x.lookup"))) uint32_t __aligned(8) rowSpriteBits[TMS9918_PIXELS_X / 32]; /* collision mask */
static __attribute__((section(".scratch_x.lookup"))) uint32_t __aligned(8) rowBits[TMS9918_PIXELS_X / 32];       /* pixel mask */

/* Function:  tmsTestCollisionMask
 * ----------------------------------------
 * Test and update the sprite collision mask.
 */
static inline uint32_t tmsTestCollisionMask(VR_EMU_INST_ARG const uint32_t xPos, const uint32_t spritePixels, const uint32_t spriteWidth, const bool update)
{
  if (!update && !tms9918->scanlineHasSprites) return spritePixels;

  uint32_t rowSpriteBitsWord = xPos >> 5;
  uint32_t rowSpriteBitsWordBit = xPos & 0x1f;
  
  uint32_t validPixels = (~rowSpriteBits[rowSpriteBitsWord]) & (spritePixels >> rowSpriteBitsWordBit);
  if (update) rowSpriteBits[rowSpriteBitsWord] |= validPixels;
  validPixels <<= rowSpriteBitsWordBit;

  if ((rowSpriteBitsWordBit + spriteWidth) > 32)
  {
    rowSpriteBitsWordBit = 32 - rowSpriteBitsWordBit;
    uint32_t right = (~rowSpriteBits[++rowSpriteBitsWord]) & (spritePixels << rowSpriteBitsWordBit);
    if (update) rowSpriteBits[rowSpriteBitsWord] |= right;
    validPixels |= (right >> rowSpriteBitsWordBit);
  }

  return validPixels;
}


/* Function:  tmsTestRowBitsMask
 * ----------------------------------------
 * Test and update the row pixels bit mask.
 */
static inline uint32_t tmsTestRowBitsMask(VR_EMU_INST_ARG const uint32_t xPos, const uint32_t tilePixels, const uint32_t tileWidth, const bool update, const bool test)
{
  uint32_t rowBitsWord = xPos >> 5;
  uint32_t rowBitsWordBit = xPos & 0x1f;

  uint32_t validPixels = tilePixels >> rowBitsWordBit;
  if (test) validPixels &= (~rowBits[rowBitsWord]);
  if (update) rowBits[rowBitsWord] |= validPixels;
  if (test) validPixels <<= rowBitsWordBit;

  if ((rowBitsWordBit + tileWidth) > 32)
  {
    ++rowBitsWord;
    rowBitsWordBit = 32 - rowBitsWordBit;
    uint32_t right = (tilePixels << rowBitsWordBit);
    if (test) right &= ~rowBits[rowBitsWord];
    if (update) rowBits[rowBitsWord] |= right;
    if (test) validPixels |= (right >> rowBitsWordBit);
  }

  return test ? validPixels : tilePixels;
}

/*
 * to generate the doubled pixels required when the sprite MAG flag is set,
 * use a lookup table. generate the doubledBits lookup table when we need it
 * using doubledBitsNibble.
 */
static uint8_t  __aligned(8) doubledBitsNibble[16] = {
  0x00, 0x03, 0x0c, 0x0f,
  0x30, 0x33, 0x3c, 0x3f,
  0xc0, 0xc3, 0xcc, 0xcf,
  0xf0, 0xf3, 0xfc, 0xff
};


/* lookup for combining ecm nibbles, returning 4 pixels */
static uint32_t __aligned(8) ecmLookup[16 * 16 * 16];

static uint8_t ecmByte(bool h, bool m, bool l)
{
  return (h << 2) | (m << 1) | l;
}

/* lookup from bit planes: 333322221111 to merged palette values for four pixels
 * NOTE: The left-most pixel is stored in the least significant byte of the result
 *       because it's more efficient to offload them that way
 */
static void ecmLookupInit()
{
  for (uint16_t i = 0; i < 16 * 16 * 16; ++i)
  {
    ecmLookup[i] = (ecmByte(i & 0x800, i & 0x080, i & 0x008)) |
                   (ecmByte(i & 0x400, i & 0x040, i & 0x004) << 8) |
                   (ecmByte(i & 0x200, i & 0x020, i & 0x002) << 16) |
                   (ecmByte(i & 0x100, i & 0x010, i & 0x001) << 24);
  }
}

/* random note about how palettes are applied:
 * PR Address bit: 0 1 2 3 4 5
 * --------------------------------------
 * original mode: ps0 ps1 cs0 cs1 cs2 cs3
 * 1-bit (ECM1) : ps0 cs0 cs1 cs2 cs3 px0
 * 2-bit (ECM2) : cs0 cs1 cs2 cs3 px1 px0
 * 3-bit (ECM3) : cs0 cs1 cs2 px2 px1 px0
*/



/* lookup for doubling pixel patterns in mag mode */
static __attribute__((section(".scratch_x.lookup"))) uint16_t __aligned(8) doubledBits[256];
static void doubledBitsInit()
{
  for (int i = 0; i < 256; ++i)
  {
    doubledBits[i] = (doubledBitsNibble[(i & 0xf0) >> 4] << 8) | doubledBitsNibble[i & 0x0f];
  }
}

/* reversed bits in a byte */
static __attribute__((section(".scratch_x.lookup"))) uint8_t __aligned(8) reversedBits[256];

uint8_t reverseBits(uint8_t byte) {
    byte = (byte & 0xf0) >> 4 | (byte & 0x0f) << 4;
    byte = (byte & 0xcc) >> 2 | (byte & 0x33) << 2;
    return (byte & 0xaa) >> 1 | (byte & 0x55) << 1;
}

static void reversedBitsInit()
{
  for (int i = 0; i < 256; ++i)
  {
    reversedBits[i] = reverseBits(i);
  }
}

/* a lookup to apply a 6-bit palette to 4 bytes of a uint32_t */
static __attribute__((section(".scratch_x.lookup"))) uint32_t __aligned(8) repeatedPalette[64];
static void repeatedPaletteInit()
{
  for (int i = 0; i < 64; ++i)
  {
    repeatedPalette[i] = (i << 24) | (i << 16) | (i << 8) | i;
  }
}

/* a lookup from a 4-bit mask to a word of 8-bit masks (reversed byte order) */
static __attribute__((section(".scratch_x.lookup"))) uint32_t __aligned(8) maskExpandNibbleToWordRev[16];
static void maskExpandNibbleToWordRevInit()
{
  for (int i = 0; i < 16; ++i)
  {
    maskExpandNibbleToWordRev[i] = ((i & 0x1) ? (0xff << 24) : 0) |
                                   ((i & 0x2) ? (0xff << 16): 0) |
                                   ((i & 0x4) ? (0xff << 8): 0) |
                                   ((i & 0x8) ? 0xff : 0);
  }
}

/* bitmap bytes to mask values */
static __attribute__((section(".scratch_x.lookup"))) __aligned(4) uint8_t bitmapByteMask[256];
static __attribute__((section(".scratch_x.lookup"))) __aligned(4) uint8_t bitmapByteMaskFat[256];
static void bitmapMasksInit()
{
  for (int i = 0; i < 256; ++i)
  {
    bitmapByteMask[i] = ((bool)(i & 0xc0) << 7) | ((bool)(i & 0x30) << 6) | ((bool)(i & 0xc) << 5) | ((bool)(i & 0xc0) << 4);
    bitmapByteMaskFat[i] = ((i & 0xf0) ? 0xc0 : 0x00) | ((i & 0x0f) ? 0xc : 0x0);
  }
}


bool lookupsReady = false;
void initLookups()
{
  if (lookupsReady) return;

  ecmLookupInit();
  doubledBitsInit();
  reversedBitsInit();
  repeatedPaletteInit();
  maskExpandNibbleToWordRevInit();
  bitmapMasksInit();
  lookupsReady = true;
}

typedef union {
    uint32_t value;
    uint8_t bytes[4];
} bytes32;



/* Function:  vrEmuTms9918OutputSprites
 * ----------------------------------------
 * Output Sprites to a scanline
 */
static inline uint8_t __time_critical_func(renderSprites)(VR_EMU_INST_ARG uint8_t y, const bool spriteMag, uint8_t pixels[TMS9918_PIXELS_X])
{
  const uint8_t spriteSize = tmsSpriteSize(tms9918);
  const bool sprite16 = spriteSize == 16;
  const uint8_t spriteIdxMask = sprite16 ? 0xfc : 0xff;
  const uint8_t spriteSizePx = spriteSize * (spriteMag + 1);
  const uint16_t spriteAttrTableAddr = tmsSpriteAttrTableAddr(tms9918);
  const uint16_t spritePatternAddr = tmsSpritePatternTableAddr(tms9918);

  uint8_t spritesShown = 0;
  uint8_t tempStatus = 0x1f;

  // ecm settings
  const uint8_t ecm = (tms9918->registers[0x31] & 0x03);
  const uint8_t ecmColorOffset = (ecm == 3) ? 2 : ecm;
  const uint8_t ecmColorMask = (ecm == 3) ? 0x0e : 0x0f;
  const int ecmOffset = 0x800 >> ((tms9918->registers[0x1d] & 0xc0) >> 6);

  uint8_t pal = tms9918->registers[0x18] & 0x30;
  if (ecm == 1)
  {
    pal &= 0x20;
  }
  else if (ecm)
  {
    pal = 0;
  }

  int maxSprites = tms9918->registers[0x33];
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
    }

    uint8_t thisSpriteSize = spriteSize;
    bool thisSprite16 = sprite16;
    uint8_t thisSpriteIdxMask = spriteIdxMask;
    uint8_t thisSpriteSizePx = spriteSizePx;

    if (!sprite16 && (spriteAttr[SPRITE_ATTR_COLOR] & 0x10))
    {
      thisSpriteSize = 16;
      thisSprite16 = true;
      thisSpriteIdxMask = 0xfc;
      thisSpriteSizePx = thisSpriteSize * (spriteMag + 1);
    }

    /* check if sprite is visible on this line */
    if (pattRow < 0 || pattRow >= thisSpriteSize)
    {
      spriteAttr += SPRITE_ATTR_BYTES;
      continue;
    }

    /* have we exceeded the scanline sprite limit? */
    if (++spritesShown > MAX_SCANLINE_SPRITES)
    {
      if (((tempStatus & STATUS_5S) == 0) && 
          (tms9918->lockedMask == 0x07 || (tms9918->registers[0x32] & 0x80) == 0 || spritesShown > tms9918->registers[0x1e]))
      {
        tempStatus &= 0xe0;
        tempStatus |= STATUS_5S | spriteIdx;
      }

      if (spritesShown > tms9918->registers[0x1e])
        break;
    }

    const int16_t earlyClockOffset = (spriteAttr[SPRITE_ATTR_COLOR] & 0x80) ? -32 : 0;
    int16_t xPos = (int16_t)(spriteAttr[SPRITE_ATTR_X]) + earlyClockOffset;
    if ((xPos > TMS9918_PIXELS_X) || (-xPos > thisSpriteSizePx))
    {
      spriteAttr += SPRITE_ATTR_BYTES;
      continue;
    }

    const bool flipY = spriteAttr[SPRITE_ATTR_COLOR] & 0x20;
    const bool flipX = spriteAttr[SPRITE_ATTR_COLOR] & 0x40;
    if (flipY) pattRow = thisSpriteSize - pattRow;

    /* sprite is visible on this line */
    uint8_t spriteColor = (spriteAttr[SPRITE_ATTR_COLOR] & ecmColorMask) << ecmColorOffset;
    const uint8_t pattIdx = spriteAttr[SPRITE_ATTR_NAME] & thisSpriteIdxMask;
    uint16_t pattOffset = spritePatternAddr + pattIdx * PATTERN_BYTES + (uint16_t)pattRow;


    /* create a 32-bit mask of this sprite's pixels
     * left-aligned, so the first pixel in the sprite is the
     * MSB of spriteBits
     */
    int32_t pattMask = 0;
    uint16_t spriteBits[4] = {0}; // a 16-bit value for each ecm bit plane

    /* load up the pattern data */
    if (flipX)
    {
      /* Note: I've made the choice to branch early for some of the sprite options
              to improve performance for each case (reduce branches in loops) */
      if (thisSprite16)
      {
        int i = 0;
        do
        {
          spriteBits[i] = reversedBits[tms9918->vram[pattOffset]];
          spriteBits[i] |= reversedBits[tms9918->vram[(pattOffset + PATTERN_BYTES * 2)]] << 8;
          pattOffset += ecmOffset;
          pattMask |= spriteBits[i];
        } while (++i < ecm);
      }
      else
      {
        int i = 0;
        do
        {
          spriteBits[i] = reversedBits[tms9918->vram[pattOffset]] << 8;
          pattOffset += ecmOffset;
          pattMask |= spriteBits[i];
        } while (++i < ecm);
      }
    }
    else if (thisSprite16)
    {
      int i = 0;
      do
      {
        spriteBits[i] = tms9918->vram[pattOffset] << 8;
        spriteBits[i] |= tms9918->vram[(pattOffset + PATTERN_BYTES * 2)];
        pattOffset += ecmOffset;
        pattMask |= spriteBits[i];
      } while (++i < ecm);
    }
    else
    {
      int i = 0;
      do
      {
        spriteBits[i] = tms9918->vram[pattOffset] << 8;
        pattOffset += ecmOffset;
        pattMask |= spriteBits[i];
      } while (++i < ecm);
    }

    /* bail early if no bits to draw */
    if (!pattMask)
    {
      spriteAttr += SPRITE_ATTR_BYTES;
      continue;
    }

    if (spriteMag)
    {
      pattMask = (doubledBits[pattMask >> 8] << 16) | doubledBits[pattMask & 0xff];
    }
    else
    {
      pattMask <<= 16;
    }

    /* perform clipping operations */
    if (xPos < 0)
    {
      int16_t offset = -xPos;
      if (spriteMag) offset >>= 1; 
      for (int i = 0; i < ecm; ++i)
      {
        spriteBits[i] <<= offset;
      }
      pattMask <<= -xPos;
      
      /* bail early if no bits to draw */
      if (!pattMask)
      {
        spriteAttr += SPRITE_ATTR_BYTES;
        continue;
      }

      thisSpriteSizePx -= -xPos;
      xPos = 0;
    }
    int overflowPx = (xPos + thisSpriteSizePx) - TMS9918_PIXELS_X;
    if (overflowPx > 0)
    {
      thisSpriteSizePx -= overflowPx;
    }

    /* test and update the collision mask - signed, because we test for msb using less-than */
    int32_t validPixels = tmsTestCollisionMask(VR_EMU_INST xPos, pattMask, thisSpriteSizePx, true);
    tms9918->scanlineHasSprites = true;

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

      /* Note: Again, I've made the choice to branch early for some of the sprite options
              to improve performance for each case (reduce branches in loops) */

        if (spriteMag)
        {
          while (validPixels)
          {
            /* output the sprite pixels 4 at a time */
            if (validPixels >> 24)
            {
              uint32_t color = ecmLookup[(spriteBits[0] >> 12) |
                                        ((spriteBits[1] >> 12) << 4) |
                                        ((spriteBits[2] >> 12) << 8)];

              uint8_t pix = color & 0x7;
              if (pix) pixels[xPos] = pixels[xPos + 1] = spriteColor | pix;
              if (pix = (color >>= 8) & 0x7) pixels[xPos + 2] = pixels[xPos + 3] = spriteColor | pix;
              if (pix = (color >>= 8) & 0x7) pixels[xPos + 4] = pixels[xPos + 5] = spriteColor | pix;
              if (pix = (color >>= 8) & 0x7) pixels[xPos + 6] = pixels[xPos + 7] = spriteColor | pix;
            }
            validPixels <<= 4;
            spriteBits[0] <<= 4;
            spriteBits[1] <<= 4;
            spriteBits[2] <<= 4;
            xPos += 8;
          }
        }
        else
        {
          while (validPixels)
          {
            /* output the sprite pixels 4 at a time */
            uint8_t chunkMask = validPixels >> 28;
            if (chunkMask)
            {
              uint32_t color = ecmLookup[(spriteBits[0] >> 12) |
                                        ((spriteBits[1] >> 12) << 4) |
                                        ((spriteBits[2] >> 12) << 8)];

              uint8_t pix = color & 0x7;
              if (pix) pixels[xPos] = spriteColor | pix;
              if (pix = (color >>= 8) & 0x7) pixels[xPos + 1] = spriteColor | pix;
              if (pix = (color >>= 8) & 0x7) pixels[xPos + 2] = spriteColor | pix;
              if (pix = (color >>= 8) & 0x7) pixels[xPos + 3] = spriteColor | pix;
            }
            validPixels <<= 4;
            spriteBits[0] <<= 4;
            spriteBits[1] <<= 4;
            spriteBits[2] <<= 4;
            xPos += 4;
          }
        }
      }
      else
      {
        while (validPixels)
        {
          if (validPixels < 0)
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

static uint8_t __time_critical_func(vrEmuTms9918OutputSprites)(VR_EMU_INST_ARG uint8_t y, uint8_t pixels[TMS9918_PIXELS_X])
{
  const bool spriteMag = tmsSpriteMag(tms9918);
  if (spriteMag)
  {
    return renderSprites(VR_EMU_INST y, true, pixels);
  }
  else
  {
    return renderSprites(VR_EMU_INST y, false, pixels);
  }
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


static __attribute__((section(".scratch_y.lookup"))) uint32_t patternData[3] = {0};


static inline uint32_t* renderEcmStartTile(
  uint32_t *quadPixels,
  uint32_t tileLeftPixels,
  uint32_t tileRightPixels,
  uint32_t pattMask,
  uint32_t startPattBit,
  uint32_t shift,
  uint32_t reverseShift)
{
  const uint32_t rightMask = maskExpandNibbleToWordRev[pattMask & 0xf];

  // first tile will either take one or two nibbles depending on the shift
  if (startPattBit < 4)
  {
    const uint32_t leftMask = maskExpandNibbleToWordRev[pattMask >> 4];
    const uint32_t mask = (leftMask >> shift) | (rightMask << reverseShift);
    const uint32_t shifted = mask & ((tileLeftPixels >> shift) | (tileRightPixels << reverseShift));
    *quadPixels++ = (*quadPixels & ~mask) | shifted;
  }

  const uint32_t mask = (rightMask >> shift);
  const uint32_t shifted = mask & (tileRightPixels >> shift);
  *quadPixels = (*quadPixels & ~mask) | shifted;

  if (startPattBit == 4) ++quadPixels;

  return quadPixels;
}

static inline uint32_t* rederEcmShiftedTile(
  uint32_t *quadPixels,
  const uint32_t tileLeftPixels,
  const uint32_t tileRightPixels,
  const uint32_t pattMask,
  const uint32_t shift,
  const uint32_t reverseShift,
  const bool isTile2) 
{
  // all subsequent shifted tiles will follow and require updating 3x nibbles
  // left, middle and right where the middle nibble is always complete
  if (false && isTile2 && pattMask == 0xff) // TODO: temporarily disabled thi sbranch... do we really need it?
  {
    const uint32_t mask = maskExpandNibbleToWordRev[pattMask >> 4] << reverseShift;
    const uint32_t shifted = mask & (tileLeftPixels << reverseShift);
    *quadPixels++ = (*quadPixels & ~mask) | shifted;

    *quadPixels++ = (tileLeftPixels >> shift) | (tileRightPixels << reverseShift);
    *quadPixels = tileRightPixels >> shift;
  }
  else
  {
    const uint32_t leftMask = maskExpandNibbleToWordRev[pattMask >> 4];
    {
      const uint32_t mask = leftMask << reverseShift;
      const uint32_t shifted = mask & (tileLeftPixels << reverseShift);
      *quadPixels++ = (*quadPixels & ~mask) | shifted;
    }

    const uint32_t rightMask = maskExpandNibbleToWordRev[pattMask & 0xf];
    {
      const uint32_t shifted = (tileLeftPixels >> shift) | (tileRightPixels << reverseShift);
      const uint32_t mask = (leftMask >> shift) | (rightMask << reverseShift);
      *quadPixels++ = (*quadPixels & ~mask) | (mask & shifted);
    }

    {
      const uint32_t mask = (rightMask >> shift);
      const uint32_t shifted = mask & (tileRightPixels >> shift);
      *quadPixels = (*quadPixels & ~mask) | shifted;
    }
  }
  
  return quadPixels;
}

static inline uint32_t* renderEcmAlignedTile(uint32_t *quadPixels, const uint32_t tileLeftPixels, const uint32_t tileRightPixels, const uint32_t pattMask)
{
  // not shifted, but transparent - need to mask the two nibbles
  const uint32_t leftMask = maskExpandNibbleToWordRev[pattMask >> 4];
  *quadPixels++ = (*quadPixels & ~leftMask) | (leftMask & tileLeftPixels);

  const uint32_t rightMask = maskExpandNibbleToWordRev[pattMask & 0xf];
  *quadPixels++ = (*quadPixels & ~rightMask) | (rightMask & tileRightPixels);  

  return quadPixels;
}

static inline uint32_t quadPixelIncrement(uint32_t startPattBit)
{
  if (!startPattBit) return 2;
  return startPattBit <= 4;
}

static inline uint32_t* renderEcmTile(
  uint32_t *quadPixels,
  const uint32_t xPos,
  const uint32_t pattIdx,
  const uint8_t patternTable[],
  const uint32_t colorTableAddr,
  const uint32_t startPattBit,
  const uint32_t ecm,
  const uint32_t ecmOffset,
  const uint32_t ecmColorMask,
  const uint32_t ecmColorOffset,
  const uint32_t pal,
  const bool attrPerPos,
  const uint32_t rowOffset,
  const uint32_t pattRow,
  const uint32_t tileIndex,
  const uint32_t shift,
  const uint32_t reverseShift,
  uint32_t *lastEmpty,
  const bool isTile2,
  const bool alwaysOnTop)
{
  /* was this pattern empty? we remember the last empty pattern */
  if (*lastEmpty == pattIdx || (!isTile2 && !tmsTestRowBitsMask(xPos, -1, 8 - startPattBit, false, true)))
  {
    return quadPixels + quadPixelIncrement(startPattBit);
  }

  /* iterate over each bit of this pattern byte */
  uint32_t colorTableOffset = pattIdx;
  if (attrPerPos) colorTableOffset = rowOffset + tileIndex;
  
  const uint32_t colorByte = tms9918->vram[colorTableAddr + colorTableOffset];

  const uint32_t priority = alwaysOnTop || (colorByte & 0x80);
  const uint32_t flipX = colorByte & 0x40;
  const uint32_t flipY = colorByte & 0x20;
  const uint32_t trans = colorByte & 0x10;

  uint32_t pattOffset = pattIdx * PATTERN_BYTES;
  pattOffset += flipY ? 7 - pattRow : pattRow;

  uint32_t pattMask = trans ? 0 : 0xff;
  uint32_t leftIndex = 0, rightIndex = 0;

  if (flipX)
  {
    for (int j = 0; j < (ecm * 4); j += 4)
    {
      uint32_t patt = patternTable[pattOffset];
      if (patt)
      {
        patt = reversedBits[patt];
        leftIndex |= (patt >> 4) << j;
        rightIndex |= (patt & 0xf) << j;
        pattMask |= patt;
      }
      pattOffset += ecmOffset;
    }
  }
  else
  {
    for (int j = 0; j < (ecm * 4); j += 4)
    {
      uint32_t patt = patternTable[pattOffset];
      if (patt)
      {
        leftIndex |= (patt >> 4) << j;
        rightIndex |= (patt & 0xf) << j;
        pattMask |= patt;
      }
      pattOffset += ecmOffset;
    }
  }

  if (pattMask)
  {
    const uint32_t offset = (24 + startPattBit);
    pattMask <<= offset;
    if (!priority)
    {
      pattMask = tmsTestCollisionMask(xPos, pattMask, 8, false);
    }
    pattMask = tmsTestRowBitsMask(xPos, pattMask, 8, true, !isTile2);

    if (!pattMask)
    {
      return quadPixels + quadPixelIncrement(startPattBit);
    }

    pattMask >>= offset;

    uint32_t palette = repeatedPalette[pal | ((colorByte & ecmColorMask) << ecmColorOffset)];
    const uint32_t tileLeftPixels = ecmLookup[leftIndex] | palette;
    const uint32_t tileRightPixels = ecmLookup[rightIndex] | palette;

    if (startPattBit)
    {
      quadPixels = renderEcmStartTile(quadPixels, tileLeftPixels, tileRightPixels, pattMask, startPattBit, shift, reverseShift);
    }
    else if (shift)
    {
      quadPixels = rederEcmShiftedTile(quadPixels, tileLeftPixels, tileRightPixels, pattMask, shift, reverseShift, isTile2);
    }
    else// if (pattMask != 0xff)
    {
      quadPixels = renderEcmAlignedTile(quadPixels, tileLeftPixels, tileRightPixels, pattMask);
    }
    /*else
    {
      // not shifted, not transparent, just dump it out
      *quadPixels++ = tileLeftPixels;
      *quadPixels++ = tileRightPixels;
    }*/
  }
  else
  {
    quadPixels += quadPixelIncrement(startPattBit);
    *lastEmpty = pattIdx;
  }
  return quadPixels;
}

static inline uint8_t* renderStdTile(
  uint8_t *pixels,
  const uint32_t xPos,
  const uint32_t pattIdx,
  const uint8_t patternTable[],
  const uint32_t colorTableAddr,
  const uint32_t startPattBit,
  const uint32_t pal,
  const uint32_t pattRow,
  const uint32_t tileIndex,
  const uint32_t shift,
  const uint32_t reverseShift,
  uint32_t count,
  const bool firstTile)
{
  uint32_t pattOffset = pattIdx * PATTERN_BYTES;

  // non-ecm - either foreground or background
  int8_t pattByte = patternTable[pattOffset + pattRow] << startPattBit;
  const uint8_t colorByte = tms9918->vram[colorTableAddr + (pattIdx >> 3)];

  const uint8_t bgFgColor[] = {
    pal | tmsBgColor(tms9918, colorByte),
    pal | tmsFgColor(tms9918, colorByte)
  };

  while (count--)
  {
    *pixels++ = bgFgColor[pattByte < 0];
    pattByte <<= 1;
  }

  return pixels;
}


static inline void __time_critical_func(vrEmuF18ATileScanLine)(VR_EMU_INST_ARG const uint8_t y, const bool hpSize, uint16_t rowNamesAddr, const uint16_t rowOffset, uint8_t tileIndex, uint8_t startPattBit, const bool attrPerPos, uint8_t pal, const bool alwaysOnTop, const bool isTile2, uint8_t pixels[TMS9918_PIXELS_X])
{
  uint32_t xPos = 0;
  uint32_t lastEmpty = -1;
 
  const uint32_t pattRow = y & 0x07;  /* which pattern row (0 - 7) */
  const uint8_t* patternTable = tms9918->vram + tmsPatternTableAddr(tms9918);

  uint32_t colorTableAddr = tmsColorTableAddr(tms9918);
  if (attrPerPos) colorTableAddr |= rowNamesAddr & 0xc00;

  // for the entire scanline, we need to shift our 4-pixel words by this much
  const uint32_t shift = (startPattBit & 0x03) << 3;
  const uint32_t reverseShift = 32 - shift;
  uint32_t lastPattId = -1;

  /* iterate over each tile in this row - if' we're scrolling, add one */
  uint32_t numTiles = GRAPHICS_NUM_COLS;

  const uint32_t ecm = (tms9918->registers[0x31] & 0x30) >> 4;
  if (ecm)
  {
    const uint32_t ecmColorOffset = (ecm == 3) ? 2 : ecm;
    const uint32_t ecmColorMask = (ecm == 3) ? 0x0e : 0x0f;
    const uint32_t ecmOffset = 0x800 >> ((tms9918->registers[0x1d] & 0x0c) >> 2);
    if (ecm == 1)
    {
      pal &= 0x20;
    }
    else if (ecm)
    {
      pal = 0;
    }

    /* keep in mind when using this... the byte order will be reversed */
    uint32_t *quadPixels = (uint32_t*)pixels;

    if (startPattBit)
    {
      const uint32_t pattIdx = tms9918->vram[rowNamesAddr + tileIndex];
      quadPixels = renderEcmTile(quadPixels, xPos, pattIdx, patternTable, colorTableAddr, startPattBit, ecm, ecmOffset, ecmColorMask, ecmColorOffset, pal, attrPerPos, rowOffset, pattRow, tileIndex++, shift, reverseShift, &lastEmpty, isTile2, alwaysOnTop);
      xPos += 8 - startPattBit;
    }

    if (shift)
    {
      while (numTiles--)
      {
        /* next page? */
        if (tileIndex == GRAPHICS_NUM_COLS)
        {
          if (hpSize)
          {
            rowNamesAddr ^= 0x400;
            if (attrPerPos) colorTableAddr ^= 0x400;
          }
          tileIndex = 0;
        }
        const uint32_t pattIdx = tms9918->vram[rowNamesAddr + tileIndex];
        quadPixels = renderEcmTile(quadPixels, xPos, pattIdx, patternTable,colorTableAddr,0,ecm,ecmOffset,ecmColorMask,ecmColorOffset,pal,attrPerPos,rowOffset,pattRow,tileIndex++,shift,reverseShift,&lastEmpty,isTile2,alwaysOnTop);
        xPos += 8;
      }
    }
    else
    {
      while (numTiles--)
      {
        /* next page? */
        if (tileIndex == GRAPHICS_NUM_COLS)
        {
          if (hpSize)
          {
            rowNamesAddr ^= 0x400;
            if (attrPerPos) colorTableAddr ^= 0x400;
          }
          tileIndex = 0;
        }
        const uint32_t pattIdx = tms9918->vram[rowNamesAddr + tileIndex];
        quadPixels = renderEcmTile(quadPixels, xPos, pattIdx, patternTable,colorTableAddr,0,ecm,ecmOffset,ecmColorMask,ecmColorOffset,pal,attrPerPos,rowOffset,pattRow,tileIndex++,0,32,&lastEmpty,isTile2,alwaysOnTop);
        xPos += 8;
      }
    }
  }
  else
  {
    while (numTiles--)
    {
      const uint32_t pattIdx = tms9918->vram[rowNamesAddr + tileIndex];
      pixels = renderStdTile(pixels, xPos, pattIdx, patternTable,colorTableAddr,0,pal,pattRow,tileIndex,shift,reverseShift,8,false);
      ++tileIndex;
      xPos += 8;
    }
  }


}

static void __time_critical_func(vrEmuF18ATile1ScanLine)(VR_EMU_INST_ARG uint8_t y, uint8_t pixels[TMS9918_PIXELS_X])
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
      swapYPage = (bool)tms9918->registers[0x1d] & 0x01;
    }
   
    y = virtY;
  }

  const uint8_t tileY = y >> 3;   /* which name table row (0 - 23)... or 29 */

  /* address in name table at the start of this row */
  uint16_t rowOffset = tileY * GRAPHICS_NUM_COLS;
  uint16_t rowNamesAddr = tmsNameTableAddr(tms9918) + rowOffset;
  if (swapYPage) rowNamesAddr ^= 0x800;

  const bool attrPerPos = tms9918->registers[0x32] & 0x02;
  uint8_t pal = (tms9918->registers[0x18] & 0x03) << 4;
  uint8_t startPattBit = tms9918->registers[0x1b] & 0x07;
  uint8_t tileIndex = (tms9918->registers[0x1b] >> 3);
  bool hpSize = tms9918->registers[0x1d] & 0x02;

  vrEmuF18ATileScanLine(VR_EMU_INST y, hpSize, rowNamesAddr, rowOffset, tileIndex, startPattBit, attrPerPos, pal, false, 0, pixels);
}


static void __time_critical_func(vrEmuF18ATile2ScanLine)(VR_EMU_INST_ARG uint8_t y, uint8_t pixels[TMS9918_PIXELS_X])
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
      swapYPage = (bool)tms9918->registers[0x1d] & 0x10;
    }
   
    y = virtY;
  }


  const bool tile2Priority = !(tms9918->registers[0x32] & 0x01);

  const uint8_t tileY = y >> 3;   /* which name table row (0 - 23)... or 29 */
  const uint8_t pattRow = y & 0x07;  /* which pattern row (0 - 7) */
  int xPos = 0;

  /* address in name table at the start of this row */
  uint16_t rowOffset = tileY * GRAPHICS_NUM_COLS;
  uint16_t rowNamesAddr = tmsNameTable2Addr(tms9918) + rowOffset;
  if (swapYPage) rowNamesAddr ^= 0x800;

  const bool attrPerPos = tms9918->registers[0x32] & 0x02;
  uint8_t pal = (tms9918->registers[0x18] & 0x0c) << 2;
  uint8_t startPattBit = tms9918->registers[0x19] & 0x07;
  uint8_t tileIndex = (tms9918->registers[0x19] >> 3);
  bool hpSize = tms9918->registers[0x1d] & 0x20;

  vrEmuF18ATileScanLine(VR_EMU_INST y, hpSize, rowNamesAddr, rowOffset, tileIndex, startPattBit, attrPerPos, pal, tile2Priority, true, pixels);
}

/* Function:  vrEmuTms9918BitmapLayerScanLine
 * ----------------------------------------
 * generate an F18A bitmap layer scanline
 */
static inline void __time_critical_func(renderBitmapLayer)(VR_EMU_INST_ARG uint8_t y, bool transp, const uint8_t width, const uint16_t addr, const bool before, const uint8_t bmlCtl, uint8_t pixels[TMS9918_PIXELS_X])
{
  if (before && !transp && width == 64)
  {
    for (int i = 0; i < TMS9918_PIXELS_X / 32; ++i)
      rowBits[i] = -1;
  }

  uint8_t xPos = tms9918->registers[0x21];

  if (bmlCtl & 0x10)  // fat 4bpp pixels?
  {
    const uint8_t colorMask = 0xf0;
    const uint8_t colorOffset = 4;
    const uint8_t colorCount = 2;
    const uint8_t colorSize = 4;

    uint8_t pal = (bmlCtl & 0xc) << 2;

    for (int xOff = 0; xOff < width; ++xOff)
    {
      uint8_t data = tms9918->vram[addr + xOff];
      for (int sp = 0; sp < colorCount; ++sp)
      {
        uint8_t color = (data & colorMask);
        if (!transp || color)
        {
          uint8_t finalColour = pal | (color >> colorOffset);
          pixels[xPos] = finalColour;
          pixels[xPos + 1] = finalColour;
        }
        xPos += 2;
        data <<= colorSize;
      }    
    }
  }
  else // regular 2bpp pixels
  {
    const uint8_t colorMask = 0xc0;
    const uint8_t colorOffset = 6;
    const uint8_t colorCount = 4;
    const uint8_t colorSize = 2;

    uint8_t pal = (bmlCtl & 0xf) << 2;

    for (int xOff = 0; xOff < width; ++xOff)
    {
      uint8_t data = tms9918->vram[addr + xOff];
      for (int sp = 0; sp < colorCount; ++sp)
      {
        uint8_t color = (data & colorMask);
        if (!transp || color)
        {
          pixels[xPos] = pal | (color >> colorOffset);
        }
        ++xPos;
        data <<= colorSize;
      }    
    }
  }
}


/* Function:  vrEmuTms9918BitmapLayerScanLine
 * ----------------------------------------
 * generate an F18A bitmap layer scanline
 */
static void __time_critical_func(vrEmuTms9918BitmapLayerScanLine)(VR_EMU_INST_ARG uint8_t y, uint8_t pixels[TMS9918_PIXELS_X], bool before)
{
  /* bml enabled? */
  const uint8_t bmlCtl = tms9918->registers[0x1f];
  if (!(bmlCtl & 0x80))
    return;

  /* bml priority */
  if ((bool)(bmlCtl & 0x40) != before)
    return;

  /* bml on this scanline? */
  const uint8_t top = tms9918->registers[0x22];
  if (top > y)
    return;

  y -= top;
  if (y >= tms9918->registers[0x24])
    return;

  const uint8_t width = tms9918->registers[0x23] ? (tms9918->registers[0x23] >> 2) : 64;
  const uint16_t addr = (tms9918->registers[0x20] << 6) + (y * width);

  if (bmlCtl & 0x20) // transp
  {
    renderBitmapLayer(VR_EMU_INST y, true, width, addr, before, bmlCtl, pixels);
  }
  else
  {
    renderBitmapLayer(VR_EMU_INST y, false, width, addr, before, bmlCtl, pixels);
  }
}



/* Function:  vrEmuTms9918GraphicsIScanLine
 * ----------------------------------------
 * generate a Graphics I mode scanline
 */
static uint8_t __time_critical_func(vrEmuTms9918GraphicsIScanLine)(VR_EMU_INST_ARG uint8_t y, uint8_t pixels[TMS9918_PIXELS_X])
{
  const bool tilesDisabled = tms9918->registers[0x32] & 0x10;

  uint8_t tempStatus = vrEmuTms9918OutputSprites(VR_EMU_INST y, pixels);

  vrEmuTms9918BitmapLayerScanLine(VR_EMU_INST y, pixels, true);

  if (tms9918->registers[0x31] & 0x80) vrEmuF18ATile2ScanLine(y, pixels);

  if (!tilesDisabled) vrEmuF18ATile1ScanLine(y, pixels);

  vrEmuTms9918BitmapLayerScanLine(VR_EMU_INST y, pixels, false);

  return tempStatus;
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

  if (!lookupsReady) initLookups();

  bool dispActive = (tms9918->registers[TMS_REG_1] & TMS_R1_DISP_ACTIVE);

  /* if the display is inactive OR we have a low priority bitmap layer, we need to clear the buffer */
  //if (!dispActive || ((tms9918->registers[0x1f] & 0xc0) == 0x80))
  {
    tmsMemset(pixels, tmsMainBgColor(tms9918), TMS9918_PIXELS_X);
  }

  if (dispActive)
  {
    /* use rowSpriteBits as a mask when tiles have priority over sprites */
    for (int i = 0; i < TMS9918_PIXELS_X / 32; ++i)
    {
      rowSpriteBits[i] = 0;
      rowBits[i] = 0;
    }
    tms9918->scanlineHasSprites = false;

    switch (tmsMode(tms9918))
    {
      case TMS_MODE_GRAPHICS_I:
        tempStatus = vrEmuTms9918GraphicsIScanLine(VR_EMU_INST y, pixels);
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
      tms9918->registers [0x1e] = MAX_SPRITES; // Sprites to process
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
    if ((regIndex == 0x32) && (value & 0x80)) { // reset all registers?
      tms9918->unlockCount = 0;
      tms9918->lockedMask = 0x07;
      tmsMemset(tms9918->registers, 0, sizeof(tms9918->registers));
      tms9918->registers [0x01] = 0x40;
      tms9918->registers [0x03] = 0x10;
      tms9918->registers [0x04] = 0x01;
      tms9918->registers [0x05] = 0x0A;
      tms9918->registers [0x06] = 0x02;
      tms9918->registers [0x07] = 0xF2;
      tms9918->registers [0x1e] = MAX_SCANLINE_SPRITES; // scanline sprites
      tms9918->registers [0x30] = 1; // vram address increment register
      tms9918->registers [0x33] = MAX_SPRITES; // Sprites to process
      tms9918->registers [0x36] = 0x40;
    } else
    
    if (regIndex == 0x0F)
    {
      uint8_t statReg = (value & 0x0f);
      tms9918->status [0x0F] = statReg;
      if (value & 0x40) tms9918->startTime = time_us_64();    // reset
      if (value & 0x20) tms9918->currentTime = time_us_64();  // snap      
      else if (value & 0x10) tms9918->startTime += (tms9918->stopTime - tms9918->startTime);
      else tms9918->currentTime = tms9918->stopTime = time_us_64();

      if (statReg > 3 && statReg < 12)
      {
        uint32_t elapsed = tms9918->currentTime - tms9918->startTime;
        divmod_result_t micro = divmod_u32u32(elapsed, 1000);
        divmod_result_t milli = divmod_u32u32(to_quotient_u32(micro), 1000);

        tms9918->status[0x06] = to_remainder_u32(micro) & 0x0ff; 
        tms9918->status[0x07] = to_remainder_u32(micro) >> 8;
        tms9918->status[0x08] = to_remainder_u32(milli) & 0x0ff;
        tms9918->status[0x09] = to_remainder_u32(milli) >> 8;
        tms9918->status[0x0a] = to_quotient_u32(milli) & 0x00ff;
        tms9918->status[0x0b] = to_quotient_u32(milli) >> 8;
      }
    }
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
