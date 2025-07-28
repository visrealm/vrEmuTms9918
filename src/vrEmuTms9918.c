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
#include "hardware/dma.h"
#include "hardware/interp.h"

#include <string.h>


static unsigned int dma8 = 5;
static unsigned int dma32noinc = 6;
static unsigned int dma32inc = 7;

static __attribute__((section(".scratch_x.buffer"))) uint32_t bg; 


#if VR_EMU_TMS9918_SINGLE_INSTANCE

static VrEmuTms9918 __aligned(256) tms9918Inst;

VrEmuTms9918* tms9918 = &tms9918Inst;

// for a single scanline, we only support a single mode... so let's cache it.
vrEmuTms9918Mode tmsCachedMode = TMS_MODE_GRAPHICS_I;

/* Function:  vrEmuTms9918Init
 * --------------------
 * initialize the TMS9918 library in single-instance mode
 */
VR_EMU_TMS9918_DLLEXPORT
void __time_critical_func(vrEmuTms9918Init)()
{
  dma_channel_config cfg = dma_channel_get_default_config(dma8);
  channel_config_set_read_increment(&cfg, false);
  channel_config_set_write_increment(&cfg, true);
  channel_config_set_transfer_data_size(&cfg, DMA_SIZE_8);
  dma_channel_set_config(dma8, &cfg, false);

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
  if (TMS_REGISTER(tms9918, TMS_REG_0) & TMS_R0_MODE_GRAPHICS_II)
    return TMS_MODE_GRAPHICS_II;
  else if (TMS_REGISTER(tms9918, TMS_REG_0) & TMS_R0_MODE_TEXT_80)
    return TMS_MODE_TEXT80;
  else 
    return r1Modes [(TMS_REGISTER(tms9918, TMS_REG_1) & (TMS_R1_MODE_MULTICOLOR | TMS_R1_MODE_TEXT)) >> 3];
}

/* Function:  tmsSpriteSize
 * ----------------------------------------
 * sprite size (8 or 16)
 */
static inline uint8_t tmsSpriteSize(VrEmuTms9918* tms9918)
{
  return TMS_REGISTER(tms9918, TMS_REG_1) & TMS_R1_SPRITE_16 ? 16 : 8;
}

/* Function:  tmsSpriteMagnification
 * ----------------------------------------
 * sprite size (0 = 1x, 1 = 2x)
 */
static inline bool tmsSpriteMag(VrEmuTms9918* tms9918)
{
  return TMS_REGISTER(tms9918, TMS_REG_1) & TMS_R1_SPRITE_MAG2;
}

/* Function:  tmsNameTableAddr
 * ----------------------------------------
 * name table base address
 */
static inline uint16_t tmsNameTableAddr(VrEmuTms9918* tms9918)
{
  return (TMS_REGISTER(tms9918, TMS_REG_NAME_TABLE) & 0x0f) << 10;
}

/* Function:  tmsNameTable2Addr
 * ----------------------------------------
 * name table base address
 */
static inline uint16_t tmsNameTable2Addr(VrEmuTms9918* tms9918)
{
  return (TMS_REGISTER(tms9918, 10) & 0x0f) << 10;
}

/* Function:  tmsColorTableAddr
 * ----------------------------------------
 * color table base address
 */
static inline uint16_t tmsColorTableAddr(VrEmuTms9918* tms9918)
{
  const uint8_t mask = (tmsCachedMode == TMS_MODE_GRAPHICS_II) ? 0x80 : 0xff;

  return (TMS_REGISTER(tms9918, TMS_REG_COLOR_TABLE) & mask) << 6;
}

/* Function:  tmsColorTable2Addr
 * ----------------------------------------
 * color table base address
 */
static inline uint16_t tmsColorTable2Addr(VrEmuTms9918* tms9918)
{
  const uint8_t mask = (tmsCachedMode == TMS_MODE_GRAPHICS_II) ? 0x80 : 0xff;

  return (TMS_REGISTER(tms9918, 11) & mask) << 6;
}

/* Function:  tmsPatternTableAddr
 * ----------------------------------------
 * pattern table base address
 */
static inline uint16_t tmsPatternTableAddr(VrEmuTms9918* tms9918)
{
  const uint8_t mask = (tmsCachedMode == TMS_MODE_GRAPHICS_II) ? 0x04 : 0x07;

  return (TMS_REGISTER(tms9918, TMS_REG_PATTERN_TABLE) & mask) << 11;
}

/* Function:  tmsSpriteAttrTableAddr
 * ----------------------------------------
 * sprite attribute table base address
 */
static inline uint16_t tmsSpriteAttrTableAddr(VrEmuTms9918* tms9918)
{
  return (TMS_REGISTER(tms9918, TMS_REG_SPRITE_ATTR_TABLE) & 0x7f) << 7;
}

/* Function:  tmsSpritePatternTableAddr
 * ----------------------------------------
 * sprite pattern table base address
 */
static inline uint16_t tmsSpritePatternTableAddr(VrEmuTms9918* tms9918)
{
  return (TMS_REGISTER(tms9918, TMS_REG_SPRITE_PATT_TABLE) & 0x07) << 11;
}

/* Function:  tmsBgColor
 * ----------------------------------------
 * background color
 */
static inline vrEmuTms9918Color tmsMainBgColor(VrEmuTms9918* tms9918)
{
  return 0;//TMS_REGISTER(tms9918, TMS_REG_FG_BG_COLOR) & 0x0f;
}

/* Function:  tmsFgColor
 * ----------------------------------------
 * foreground color
 */
static inline vrEmuTms9918Color tmsMainFgColor(VrEmuTms9918* tms9918)
{
  const vrEmuTms9918Color c = (vrEmuTms9918Color)(TMS_REGISTER(tms9918, TMS_REG_FG_BG_COLOR) >> 4);
  return c;// == TMS_TRANSPARENT ? tmsMainBgColor(tms9918) : c;
}

/* Function:  tmsFgColor
 * ----------------------------------------
 * foreground color
 */
static inline vrEmuTms9918Color tmsFgColor(VrEmuTms9918* tms9918, uint8_t colorByte)
{
  const vrEmuTms9918Color c = (vrEmuTms9918Color)(colorByte >> 4);
  return c;// == TMS_TRANSPARENT ? tmsMainBgColor(tms9918) : c;
}

/* Function:  tmsBgColor
 * ----------------------------------------
 * background color
 */
static inline vrEmuTms9918Color tmsBgColor(VrEmuTms9918* tms9918, uint8_t colorByte)
{
  const vrEmuTms9918Color c = (vrEmuTms9918Color)(colorByte & 0x0f);
  return c;// == TMS_TRANSPARENT ? tmsMainBgColor(tms9918) : c;
}


/* Function:  tmsMemset
 * ----------------------------------------
 * memset using rp2040 hardware dma
 */
static void tmsMemset(uint8_t* ptr, uint8_t val8, int count, bool wait)
{
  dma_channel_set_read_addr(dma8, &val8, false);
  dma_channel_set_trans_count(dma8, count, false);
  dma_channel_set_write_addr(dma8, ptr, true);
  if (wait) dma_channel_wait_for_finish_blocking(dma8);
}

// default palette 0xARGB
static const uint16_t defaultPalette[] = {
  //-- Palette 0, Deafult TMS9918A palette
  0x0000, 0xF000, 0xF2C3, 0xF5D6, 0xF54F, 0xF76F, 0xFD54, 0xF4EF, 0xFF54, 0xFF76, 0xFDC3, 0xFED6, 0xF2B2, 0xFC5C, 0xFCCC, 0xFFFF,
  //-- Palette 1, ECM1 (0 index is always 000) version of palette 0
  0x0000, 0xF2C3, 0xF000, 0xF54F, 0xF000, 0xFD54, 0xF000, 0xF4EF, 0xF000, 0xFCCC, 0xF000, 0xFDC3, 0xF000, 0xFC5C, 0xF000, 0xFFFF,
  //-- Palette 2, CGA colors
  0x0000, 0xF00A, 0xF0A0, 0xF0AA, 0xFA00, 0xFA0A, 0xFA50, 0xFAAA, 0xF555, 0xF55F, 0xF5F5, 0xF5FF, 0xFF55, 0xFF5F, 0xFFF5, 0xFFFF,
  //-- Palette 3, ECM1 (0 index is always 000) version of palette 2
  0x0000, 0xF555, 0xF000, 0xF00A, 0xF000, 0xF0A0, 0xF000, 0xF0AA, 0xF000, 0xFA00, 0xF000, 0xFA0A, 0xF000, 0xFA50, 0xF000, 0xFFFF
};

static void __attribute__ ((noinline)) vdpRegisterReset(VrEmuTms9918* tms9918)
{
  tms9918->isUnlocked = false;
  tms9918->restart = 0;
  tms9918->unlockCount = 0;
  tms9918->lockedMask = 0x07;
  memset(&TMS_REGISTER(tms9918, 0), 0, TMS_REGISTERS);
  TMS_REGISTER(tms9918, 0x01) = 0x40;
  TMS_REGISTER(tms9918, 0x03) = 0x10;
  TMS_REGISTER(tms9918, 0x04) = 0x01;
  TMS_REGISTER(tms9918, 0x05) = 0x0A;
  TMS_REGISTER(tms9918, 0x06) = 0x02;
  TMS_REGISTER(tms9918, 0x07) = 0xF2;
  TMS_REGISTER(tms9918, 0x1e) = MAX_SPRITES - 1; // scanline sprites
  TMS_REGISTER(tms9918, 0x30) = 1; // vram address increment register
  TMS_REGISTER(tms9918, 0x33) = MAX_SPRITES; // Sprites to process
  TMS_REGISTER(tms9918, 0x36) = 0x40;
}


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
  memset(&TMS_STATUS(tms9918, 0), 0, TMS_STATUS_REGISTERS);
  TMS_STATUS(tms9918, 0) = 0x1f;
  TMS_STATUS(tms9918, 1) = 0xE8;  // ID = F18A (0xE0) set 0x08 for anyone who cares it's not a real one
  TMS_STATUS(tms9918, 14) = 0x1A; // Version
  tms9918->readAheadBuffer = 0;

  vdpRegisterReset(tms9918);
  TMS_REGISTER(tms9918, 0x01) = 0x00; // turn display off
  TMS_REGISTER(tms9918, 0x07) = 0x00;
  tmsCachedMode = TMS_MODE_GRAPHICS_I;

  // set up default palettes (arm is little-endian, tms9900 is big-endian)
  for (int i = 0; i < sizeof(defaultPalette) / sizeof(uint16_t); ++i)
  {
    tms9918->vram.map.pram[i] = __builtin_bswap16(defaultPalette[i]);
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

typedef uint32_t BitMask[9];


static __attribute__((section(".scratch_x.lookup"))) BitMask __aligned(4) rowSpriteBits;             /* collision mask */
static __attribute__((section(".scratch_x.lookup"))) BitMask __aligned(4) rowTransparentSpriteBits;  /* transparent sprite pixels */
static __attribute__((section(".scratch_x.lookup"))) BitMask __aligned(4) rowBits;                   /* pixel mask */

/* Function:  tmsTestCollisionMask
 * ----------------------------------------
 * Test and update the sprite collision mask.
 */
static inline uint32_t tmsTestCollisionMask(const uint32_t xPos, const uint32_t spritePixels, const uint32_t spriteWidth)
{
  uint32_t rowSpriteBitsWord = xPos >> 5;
  uint32_t rowSpriteBitsWordBit = xPos & 0x1f;

  uint32_t validPixels = (~rowSpriteBits[rowSpriteBitsWord]) & (spritePixels >> rowSpriteBitsWordBit);
  rowSpriteBits[rowSpriteBitsWord] |= validPixels;
  validPixels <<= rowSpriteBitsWordBit;

  rowSpriteBitsWordBit = 32 - rowSpriteBitsWordBit;
  if (rowSpriteBitsWordBit < spriteWidth)
  {
    uint32_t right = (~rowSpriteBits[++rowSpriteBitsWord]) & (spritePixels << rowSpriteBitsWordBit);
    rowSpriteBits[rowSpriteBitsWord] |= right;
    validPixels |= (right >> rowSpriteBitsWordBit);
  }

  return validPixels;
}


/* Function:  tmsSetTransparentSpriteMask
 * ----------------------------------------
 * set the transparent sprite mask.
 */
static inline void tmsSetTransparentSpriteMask(const uint32_t xPos, const uint32_t spritePixels, const uint32_t spriteWidth)
{
  uint32_t rowSpriteBitsWord = xPos >> 5;
  uint32_t rowSpriteBitsWordBit = xPos & 0x1f;
  
  rowTransparentSpriteBits[rowSpriteBitsWord] |= spritePixels >> rowSpriteBitsWordBit;

  rowSpriteBitsWordBit = 32 - rowSpriteBitsWordBit;
  if (rowSpriteBitsWordBit < spriteWidth)
  {
    rowTransparentSpriteBits[rowSpriteBitsWord + 1] |= spritePixels << rowSpriteBitsWordBit;
  }
}


/* Function:  tmsUpdateRowBitsMask
  * ----------------------------------------
  * Update the row pixels bit mask.
  */
static inline void tmsUpdateRowBitsMask(const uint32_t xPos, const uint32_t tilePixels, const
uint32_t tileWidth, BitMask rowBitsMask)
{
  uint32_t rowBitsWord = xPos >> 5;
  uint32_t rowBitsWordBit = xPos & 0x1f;

  uint32_t validPixels = tilePixels >> rowBitsWordBit;
  rowBitsMask[rowBitsWord] |= validPixels;

  rowBitsWordBit = 32 - rowBitsWordBit;
  if (rowBitsWordBit < tileWidth) {
    ++rowBitsWord;
    uint32_t right = (tilePixels << rowBitsWordBit);
    rowBitsMask[rowBitsWord] |= right;
  }
}

/* Function:  tmsTestRowBitsMask
  * ----------------------------------------
  * Test against the row pixels bit mask.
  */
static inline uint32_t tmsTestRowBitsMask2(const uint32_t xPos, const uint32_t tilePixels, const
uint32_t tileWidth, const BitMask rowBitsMask)
{
  uint32_t rowBitsWord = xPos >> 5;
  uint32_t rowBitsWordBit = xPos & 0x1f;

  uint32_t validPixels = tilePixels >> rowBitsWordBit;
  validPixels &= ~rowBitsMask[rowBitsWord];
  validPixels <<= rowBitsWordBit;

  rowBitsWordBit = 32 - rowBitsWordBit;
  if (rowBitsWordBit < tileWidth) {
    ++rowBitsWord;
    uint32_t right = (tilePixels << rowBitsWordBit);
    right &= ~rowBitsMask[rowBitsWord];
    validPixels |= (right >> rowBitsWordBit);
  }

  return validPixels;
}

/* Function:  tmsTestAndUpdateRowBitsMask
  * ----------------------------------------
  * Test and update the row pixels bit mask.
  */
static inline uint32_t tmsTestAndUpdateRowBitsMask(const uint32_t xPos, const uint32_t
tilePixels, const uint32_t tileWidth, BitMask rowBitsMask)
{
  uint32_t rowBitsWord = xPos >> 5;
  uint32_t rowBitsWordBit = xPos & 0x1f;

  uint32_t validPixels = tilePixels >> rowBitsWordBit;
  validPixels &= ~rowBitsMask[rowBitsWord];
  rowBitsMask[rowBitsWord] |= validPixels;
  validPixels <<= rowBitsWordBit;

  rowBitsWordBit = 32 - rowBitsWordBit;
  if (rowBitsWordBit < tileWidth) {
    ++rowBitsWord;
    uint32_t right = (tilePixels << rowBitsWordBit);
    right &= ~rowBitsMask[rowBitsWord];
    rowBitsMask[rowBitsWord] |= right;
    validPixels |= (right >> rowBitsWordBit);
  }

  return validPixels;
}

/* Function:  tmsUpdateRowBitsMaskAligned
  * ----------------------------------------
  * Update the row pixels bit mask (aligned - no word boundary crossing).
  */
static inline void tmsUpdateRowBitsMaskAligned(const uint32_t xPos, const uint32_t tilePixels,
BitMask rowBitsMask)
{
  rowBitsMask[xPos >> 5] |= tilePixels >> (xPos & 0x1f);
}

/* Function:  tmsTestRowBitsMaskAligned
  * ----------------------------------------
  * Test against the row pixels bit mask (aligned - no word boundary crossing).
  */
static inline uint32_t tmsTestRowBitsMaskAligned(const uint32_t xPos, const uint32_t tilePixels,
const BitMask rowBitsMask)
{
  uint32_t rowBitsWordBit = xPos & 0x1f;

  uint32_t validPixels = tilePixels >> rowBitsWordBit;
  validPixels &= ~rowBitsMask[xPos >> 5];
  validPixels <<= rowBitsWordBit;

  return validPixels;
}

/* Function:  tmsTestAndUpdateRowBitsMaskAligned
  * ----------------------------------------
  * Test and update the row pixels bit mask (aligned - no word boundary crossing).
  */
static inline uint32_t tmsTestAndUpdateRowBitsMaskAligned(const uint32_t xPos, const uint32_t
tilePixels, BitMask rowBitsMask)
{
  uint32_t rowBitsWord = xPos >> 5;
  uint32_t rowBitsWordBit = xPos & 0x1f;

  uint32_t validPixels = tilePixels >> rowBitsWordBit;
  validPixels &= ~rowBitsMask[rowBitsWord];
  rowBitsMask[rowBitsWord] |= validPixels;
  validPixels <<= rowBitsWordBit;

  return validPixels;
}  


// Copy and align bit mask with small pixel offset (0-7)
static void tmsCopyAlignMask(BitMask dstMask, const BitMask srcMask, int pixelShift)
{
  if (pixelShift == 0)
  {
    memcpy(dstMask, srcMask, 32);
    return;
  }

  if (pixelShift > 0)
  {
    // Right shift
    uint32_t carry = 0;
    for (int i = 8; i >= 0; i--) {
      uint32_t word = srcMask[i];
      dstMask[i] = (word >> pixelShift) | carry;
      carry = word << (32 - pixelShift);
    }
  }
  else
  {
    // Left shift
    pixelShift = -pixelShift;
    uint32_t carry = 0;
    for (int i = 0; i < 9; i++) {
      uint32_t word = srcMask[i];
      dstMask[i] = (word << pixelShift) | carry;
      carry = word >> (32 - pixelShift);
    }
  }
}

// Shift srcMask by pixelShift and OR it into dstMask
static void tmsShiftMergeMasks(BitMask dstMask, const BitMask srcMask, int pixelShift)
{
  if (pixelShift == 0)
  {
    // Just OR without shifting
    for (int i = 0; i < 9; i++)
    {
      dstMask[i] |= srcMask[i];
    }
    return;
  }

  if (pixelShift > 0)
  {
    // Right shift and merge
    uint32_t carry = 0;
    for (int i = 8; i >= 0; i--)
    {
      uint32_t word = srcMask[i];
      dstMask[i] |= (word >> pixelShift) | carry;
      carry = word << (32 - pixelShift);
    }
  }
  else
  {
    // Left shift and merge
    pixelShift = -pixelShift;
    uint32_t carry = 0;
    for (int i = 0; i < 9; i++)
    {
      uint32_t word = srcMask[i];
      dstMask[i] |= (word << pixelShift) | carry;
      carry = word >> (32 - pixelShift);
    }
  }
}


/* Function:  tmsTestRowBitsMask
 * ----------------------------------------
 * Test and update the row pixels bit mask.
 */
static inline uint32_t tmsTestRowBitsMask(VR_EMU_INST_ARG const uint32_t xPos, const uint32_t tilePixels, const uint32_t tileWidth, const bool update, const bool test, const bool testColl)
{
  uint32_t rowBitsWord = xPos >> 5;
  uint32_t rowBitsWordBit = xPos & 0x1f;

  uint32_t validPixels = tilePixels >> rowBitsWordBit;
  if (testColl) validPixels &= ~rowSpriteBits[rowBitsWord];
  if (test) validPixels &= ~rowBits[rowBitsWord];
  if (update) rowBits[rowBitsWord] |= validPixels;
  if (test || testColl) validPixels <<= rowBitsWordBit;
  
  rowBitsWordBit = 32 - rowBitsWordBit;
  if (rowBitsWordBit < tileWidth)
  {
    ++rowBitsWord;
    uint32_t right = (tilePixels << rowBitsWordBit);

    if (testColl) right &= ~rowSpriteBits[rowBitsWord];
    if (test) right &= ~rowBits[rowBitsWord];

    if (update) rowBits[rowBitsWord] |= right;
    if (test || testColl) validPixels |= (right >> rowBitsWordBit);
  }

  return (test || testColl) ? validPixels : tilePixels;
}


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


/*
 * to generate the doubled pixels required when the sprite MAG flag is set,
 * use a lookup table. generate the doubledBits lookup table when we need it
 * using doubledBitsNibble.
 */
static uint8_t  __aligned(4) doubledBitsNibble[16] = {
  0x00, 0x03, 0x0c, 0x0f,
  0x30, 0x33, 0x3c, 0x3f,
  0xc0, 0xc3, 0xcc, 0xcf,
  0xf0, 0xf3, 0xfc, 0xff
};

/* lookup for doubling pixel patterns in mag mode */
static __attribute__((section(".scratch_x.lookup"))) uint16_t __aligned(4) doubledBits[256];
static void doubledBitsInit()
{
  for (int i = 0; i < 256; ++i)
  {
    doubledBits[i] = (doubledBitsNibble[(i & 0xf0) >> 4] << 8) | doubledBitsNibble[i & 0x0f];
  }
}

/* reversed bits in a byte */
static __attribute__((section(".scratch_x.lookup"))) uint8_t __aligned(4) reversedBits[256];

static uint8_t reverseBits(uint8_t byte) {
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
static __attribute__((section(".scratch_x.lookup"))) uint32_t __aligned(4) repeatedPalette[64];
static void repeatedPaletteInit()
{
  for (int i = 0; i < 64; ++i)
  {
    repeatedPalette[i] = (i << 24) | (i << 16) | (i << 8) | i;
  }
}

/* a lookup from a 4-bit mask to a word of 8-bit masks (reversed byte order) */
static __attribute__((section(".scratch_x.lookup"))) uint32_t __aligned(4) maskExpandNibbleToWordRev[16] =
{
  0x00000000, 0xff000000, 0x00ff0000, 0xffff0000,
  0x0000ff00, 0xff00ff00, 0x00ffff00, 0xffffff00,
  0x000000ff, 0xff0000ff, 0x00ff00ff, 0xffff00ff,
  0x0000ffff, 0xff00ffff, 0x00ffffff, 0xffffffff
};

bool lookupsReady = false;
void initLookups()
{
  if (lookupsReady) return;

  ecmLookupInit();
  doubledBitsInit();
  reversedBitsInit();
  repeatedPaletteInit();
  lookupsReady = true;
}

static inline void loadSpriteData(uint32_t *spriteBits, uint32_t pattOffset, uint32_t *pattMask, const uint32_t ecm, const uint32_t ecmOffset, const bool flipX, const bool sprite16)
{
  int i = 0;
  uint32_t patt;
  do  // do-while since behavior for ecm=0 and ecm==1 is the same
  {
    patt = tms9918->vram.bytes[pattOffset];
    if (flipX) patt = reversedBits[patt];
    spriteBits[i] = patt << ((flipX && sprite16) ? 16 : 24);
    if (sprite16)
    {
      patt = tms9918->vram.bytes[pattOffset + PATTERN_BYTES * 2];
      if (flipX) patt = reversedBits[patt];
      spriteBits[i] |= patt << (flipX ? 24 : 16);
    }
    *pattMask |= spriteBits[i];
    pattOffset += ecmOffset;
  } while (++i < ecm);
}


/* Function:  vrEmuTms9918OutputSprites
 * ----------------------------------------
 * Output Sprites to a scanline
 */
static inline uint8_t __time_critical_func(renderSprites)(VR_EMU_INST_ARG uint8_t y, const bool spriteMag, uint8_t pixels[TMS9918_PIXELS_X])
{
  const uint8_t spriteSize = tmsSpriteSize(tms9918);
  const bool sprite16 = spriteSize == 16;
  const uint8_t spriteIdxMask = sprite16 ? 0xfc : 0xff;
  const uint8_t spriteSizePx = spriteSize << spriteMag;
  const uint16_t spriteAttrTableAddr = tmsSpriteAttrTableAddr(tms9918);
  const uint16_t spritePatternAddr = tmsSpritePatternTableAddr(tms9918);
  const bool row30Mode = tms9918->isUnlocked && (TMS_REGISTER(tms9918, 0x31) & 0x40);
  const uint32_t maxY = row30Mode ? 0xf0 : 0xe0;

  uint32_t spritesShown = 0;
  uint8_t tempStatus = 0x1f;
  uint32_t transparentCount = 0;

  // ecm settings  
  const uint32_t ecm = tms9918->isUnlocked ? (TMS_REGISTER(tms9918, 0x31) & 0x03) : 0;
  const uint32_t ecmColorOffset = (ecm == 3) ? 2 : ecm;
  const uint32_t ecmColorMask = (ecm == 3) ? 0x0e : 0x0f;
  const uint32_t ecmOffset = 0x800 >> ((TMS_REGISTER(tms9918, 0x1d) & 0xc0) >> 6);

  uint8_t pal = 0;
  if (tms9918->isUnlocked) pal = TMS_REGISTER(tms9918, 0x18) & 0x30;
  if (ecm == 1)
  {
    pal &= 0x20;
  }
  else if (ecm)
  {
    pal = 0;
  }

  uint32_t maxSprites = TMS_REGISTER(tms9918, 0x33);
  if (maxSprites > MAX_SPRITES) maxSprites = MAX_SPRITES;

  const int32_t realY = (TMS_REGISTER(tms9918, 0x31) & 0x08) ? 0 : 1;

  uint8_t* spriteAttr = tms9918->vram.bytes + spriteAttrTableAddr;
  for (uint32_t spriteIdx = 0; spriteIdx < maxSprites; ++spriteIdx)
  {
    /* Kidd-proofing: not strictly correct, however some F18A games (lookin' at you, Kidd) 
       sometimes have all sprites enabled with all zeros and that hurts us :( */
    if (*(uint32_t*)spriteAttr == 0 && tms9918->isUnlocked)
    {
      spriteAttr += SPRITE_ATTR_BYTES;
      continue;
    }

    int32_t yPos = spriteAttr[SPRITE_ATTR_Y];

    /* stop processing when yPos == LAST_SPRITE_YPOS */
    if (yPos == LAST_SPRITE_YPOS && !row30Mode)
    {
      break;
    }

    /* first row is YPOS -1 (0xff). 2nd row is YPOS 0 */
    yPos += realY;

    /* check if sprite position is in the -31 to 0 range and move back to top */
    if (yPos > maxY)
      yPos -= 256;

    int32_t pattRow = y - yPos;
    if (pattRow < 0)
    {
      spriteAttr += SPRITE_ATTR_BYTES;
      continue;
    }

    pattRow >>= spriteMag;  // this needs to be a shift because -1 / 2 becomes 0. Bad.

    uint8_t thisSpriteSize = spriteSize;
    bool thisSprite16 = sprite16;
    uint8_t thisSpriteIdxMask = spriteIdxMask;
    uint8_t thisSpriteSizePx = spriteSizePx;
    uint8_t spriteAttrColor = spriteAttr[SPRITE_ATTR_COLOR];
    if (!tms9918->isUnlocked) spriteAttrColor &= 0x8f;

    if (!sprite16 && (spriteAttrColor & 0x10))
    {
      thisSpriteSize = 16;
      thisSprite16 = true;
      thisSpriteIdxMask = 0xfc;
      thisSpriteSizePx = thisSpriteSize << spriteMag;
    }

    /* check if sprite is visible on this line */
    if (pattRow >= thisSpriteSize)
    {
      spriteAttr += SPRITE_ATTR_BYTES;
      continue;
    }

    /* have we exceeded the scanline sprite limit? */
    if (++spritesShown > MAX_SCANLINE_SPRITES)
    {
      if (((tempStatus & STATUS_5S) == 0) && 
          (!tms9918->isUnlocked || (TMS_REGISTER(tms9918, 0x32) & 0x08) == 0 || spritesShown > TMS_REGISTER(tms9918, 0x1e)))
      {
        tempStatus &= 0xe0;
        tempStatus |= STATUS_5S | spriteIdx;
      }

      if (spritesShown > TMS_REGISTER(tms9918, 0x1e))
        break;
    }

    const int32_t earlyClockOffset = (spriteAttrColor & 0x80) ? -32 : 0;
    int32_t xPos = (int32_t)(spriteAttr[SPRITE_ATTR_X]) + earlyClockOffset;
    if ((xPos > TMS9918_PIXELS_X) || (-xPos > thisSpriteSizePx))
    {
      spriteAttr += SPRITE_ATTR_BYTES;
      continue;
    }

    if (spriteAttrColor & 0x20) pattRow = thisSpriteSize - pattRow; // flip Y?

    /* sprite is visible on this line */
    uint8_t spriteColor = (spriteAttrColor & ecmColorMask) << ecmColorOffset;
    const uint8_t pattIdx = spriteAttr[SPRITE_ATTR_NAME] & thisSpriteIdxMask;
    uint16_t pattOffset = spritePatternAddr + pattIdx * PATTERN_BYTES + (uint16_t)pattRow;


    /* create a 32-bit mask of this sprite's pixels
     * left-aligned, so the first pixel in the sprite is the
     * MSB of spriteBits
     */
    uint32_t pattMask = 0;
    uint32_t spriteBits[3] = {0}; // a 32-bit value for each ecm bit plane (also pushed far left)
    const bool flipX = spriteAttrColor & 0x40;

    if (flipX)
      if (thisSprite16)
        loadSpriteData(spriteBits, pattOffset, &pattMask, ecm, ecmOffset, true, true);
      else
        loadSpriteData(spriteBits, pattOffset, &pattMask, ecm, ecmOffset, true, false);
    else
      if (thisSprite16)
      {
        loadSpriteData(spriteBits, pattOffset, &pattMask, ecm, ecmOffset, false, true);
      }
      else
      {
        loadSpriteData(spriteBits, pattOffset, &pattMask, ecm, ecmOffset, false, false);
      }

    /* bail early if no bits to draw */
    if (!pattMask)
    {
      spriteAttr += SPRITE_ATTR_BYTES;
      continue;
    }

    if (spriteMag)
    {
      pattMask = (doubledBits[pattMask >> 24] << 16) | doubledBits[(pattMask >> 16) & 0xff];
    }

    /* perform clipping operations */
    if (xPos < 0)
    {
      int32_t absX = -xPos;
      uint32_t offset = absX >> spriteMag;
      switch (ecm)
      {
        case 3:
          spriteBits[2] <<= offset;
          // fallthrough
        case 2:
          spriteBits[1] <<= offset;
          // fallthrough
        default:
          spriteBits[0] <<= offset;
      }
      pattMask <<= absX;
      
      /* bail early if no bits to draw */
      if (!pattMask)
      {
        spriteAttr += SPRITE_ATTR_BYTES;
        continue;
      }

      thisSpriteSizePx += xPos;
      xPos = 0;
    }
    int overflowPx = (xPos + thisSpriteSizePx) - TMS9918_PIXELS_X;
    if (overflowPx > 0)
    {
      thisSpriteSizePx -= overflowPx;
    }

    /* test and update the collision mask */
    uint32_t validPixels = tmsTestCollisionMask(VR_EMU_INST xPos, pattMask, thisSpriteSizePx);

    /* if the result is different, we collided */
    if (validPixels != pattMask)
    {
      tempStatus |= STATUS_COL;
    }

    // Render valid pixels to the scanline
    if (spriteColor != TMS_TRANSPARENT)
    {
      tms9918->scanlineHasSprites = true;
      spriteColor |= pal;
      if (ecm)
      {

      /* Note: Again, I've made the choice to branch early for some of the sprite options
              to improve performance for each case (reduce branches in loops) */
        uint32_t quadPal = repeatedPalette[spriteColor];

        bool singlePix = spriteMag && thisSprite16;

         // 16px magnified is separate because it's harder. we only have 32 bits to play with, so to word align it, we need to go to 64 bits
        if (singlePix)
        {
          register uint32_t sb0 = spriteBits[0];
          register uint32_t sb1 = spriteBits[1];
          register uint32_t sb2 = spriteBits[2];

          while (validPixels)
          {
            /* output the sprite pixels 8 at a time (4x magnified pixels) */
            uint32_t chunkMask = validPixels >> 24;
            if (chunkMask)
            {
              uint32_t ecmIndex = 0;
              switch (ecm)
              {
                case 3:
                  ecmIndex = (sb2 >> 28) << 8;
                  // fallthrough
                case 2:
                  ecmIndex |= (sb1 >> 28) << 4;
                  // fallthrough
                default:
                  ecmIndex |= sb0 >> 28;
              }

              uint32_t color = ecmLookup[ecmIndex] | quadPal;

              uint8_t *p = pixels + xPos;
              if (chunkMask & 0x80) p[0] = color;
              if (chunkMask & 0x40) p[1] = color;
              color >>= 8;
              if (chunkMask & 0x20) p[2] = color;
              if (chunkMask & 0x10) p[3] = color;
              color >>= 8;
              if (chunkMask & 0x8) p[4] = color;
              if (chunkMask & 0x4) p[5] = color;
              color >>= 8;
              if (chunkMask & 0x2) p[6] = color;
              if (chunkMask & 0x1) p[7] = color;
            }
            sb2 <<= 4;
            sb1 <<= 4;
            sb0 <<= 4;
            validPixels <<= 8;
            xPos += 8;
          }
        }
        else  // regular ecm sprite (8 or 16px, non-magnified) or 8px magnified
        {
          if (spriteMag)
          {
            spriteBits[2] = doubledBits[spriteBits[2] >> 24] << 16;
            spriteBits[1] = doubledBits[spriteBits[1] >> 24] << 16;
            spriteBits[0] = doubledBits[spriteBits[0] >> 24] << 16;
          }

          // get him to be word aligned so we can smash out 4 pixels at a time
          uint32_t quadOffset = xPos >> 2;
          const uint32_t pixOffset = xPos & 0x3;
          validPixels >>= pixOffset;
          spriteBits[2] >>= pixOffset;
          spriteBits[1] >>= pixOffset;
          spriteBits[0] >>= pixOffset;

          uint32_t *quadPixels = (uint32_t*)pixels;

          while (validPixels)
          {
            /* output the sprite 4 pixels at a time */
            uint32_t chunkMask = validPixels >> 28;
            if (chunkMask)
            {
              uint32_t ecmIndex = 0;
              switch (ecm)
              {
                case 3:
                  ecmIndex |= (spriteBits[2] >> 28) << 8;
                  // fallthrough
                case 2:
                  ecmIndex |= (spriteBits[1] >> 28) << 4;
                  // fallthrough
                default:
                  ecmIndex |= spriteBits[0] >> 28;
              }

              uint32_t color = ecmLookup[ecmIndex] | quadPal;

              uint32_t maskQuad = maskExpandNibbleToWordRev[chunkMask];

              quadPixels[quadOffset] = (quadPixels[quadOffset] & ~maskQuad) | (color & maskQuad);
            }
            spriteBits[2] <<= 4;
            spriteBits[1] <<= 4;
            spriteBits[0] <<= 4;
            ++quadOffset;
            validPixels <<= 4;
          }
        }
      }
      else  // non-ecm single-color sprite
      {
        if (tmsCachedMode == TMS_MODE_TEXT80) spriteColor |= spriteColor << 4;

        while (validPixels)
        {
          if ((int32_t)validPixels < 0)
          {
            pixels[xPos] = spriteColor;
          }
          validPixels <<= 1;
          ++xPos;
        }          
      }
    }
    else
    {
      // keep track of the transparent sprites, because we want to remove them from the rowSpriteBits mask later
      if (!transparentCount)
      {
        for (int i = 0; i < 8; ++i) // clear rowTransparentSpriteBits only when we need it
        {
          rowTransparentSpriteBits[i] = 0;
        }
      }
      tmsSetTransparentSpriteMask(xPos, validPixels, thisSpriteSizePx);
      ++transparentCount;
    }

    spriteAttr += SPRITE_ATTR_BYTES;
  }

  // remove the transparent sprite pixels if there are any
  if (transparentCount)
  {
    for (int i = 0; i < 8; ++i)
    {
      rowSpriteBits[i] ^= rowTransparentSpriteBits[i];
    }
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
  uint8_t* rowNamesTable = tms9918->vram.bytes + tmsNameTableAddr(tms9918) + tileY * TEXT_NUM_COLS;
  const uint8_t* patternTable = tms9918->vram.bytes + tmsPatternTableAddr(tms9918) + pattRow;

  const uint32_t bgFgColor[2] = {
    tmsMainBgColor(tms9918),
    tmsMainFgColor(tms9918)
  };

  uint8_t* pixPtr = pixels;

  pixPtr += TEXT_PADDING_PX;

  dma_channel_wait_for_finish_blocking(dma32noinc); 

  for (uint8_t tileX = 0; tileX < TEXT_NUM_COLS; ++tileX)
  {
    const uint8_t pattByte = patternTable[*rowNamesTable++ * PATTERN_BYTES];

    for (uint8_t pattBit = 7; pattBit > 1; --pattBit)
    {
      *pixPtr++ = bgFgColor[(pattByte >> pattBit) & 0x01];
    }
  }
}

static const uint8_t __aligned(4) maskText80Fg[] = { 0x00, 0x0f, 0xf0, 0xff };
static const uint8_t __aligned(4) maskText80Dual[] = { 0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff };

static void renderText80Layer(
  uint8_t y, const uint8_t tileY, uint8_t* rowNamesTable, const uint8_t *patternTable, uint8_t* colorTable, const bool opaq, uint8_t *pixels)
{
  int initialFgBgTransp = opaq * 3;
  for (uint8_t tileX = 0; tileX < TEXT80_NUM_COLS; ++tileX)
  {
    const uint32_t pattId = *rowNamesTable++;
    
    uint32_t pattByte = patternTable[pattId * PATTERN_BYTES];

    const uint32_t colorByte = colorTable[tileX];
    uint8_t fgColor = colorByte >> 4;
    uint8_t bgColor = colorByte & 0xf;

    if (pattByte == 0 && !opaq && bgColor == 0)
    {
      pixels += TEXT_CHAR_WIDTH / 2;
      continue;
    }

    if (opaq)
    {
      const uint8_t bgFgColor[4] =
      {
        (bgColor << 4) | bgColor,
        (bgColor << 4) | fgColor,
        (fgColor << 4) | bgColor,
        (fgColor << 4) | fgColor
      };

      for (uint8_t pattBit = 6; pattBit > 1; pattBit -= 2)
      {
        *pixels++ = bgFgColor[(pattByte >> pattBit) & 0x03];
      }
    }
    else
    {
      fgColor = maskText80Dual[fgColor];
      bgColor = maskText80Dual[bgColor];

      int fgBgTransp = initialFgBgTransp | ((bool)fgColor << 1) | ((bool)bgColor);
      if (!fgBgTransp)
      {
        pixels += TEXT_CHAR_WIDTH / 2;
        continue;
      }


      uint32_t mask3 = maskText80Fg[(pattByte >> 2) & 0x03];
      uint32_t mask2 = maskText80Fg[(pattByte >> 4) & 0x03];
      uint32_t mask1 = maskText80Fg[pattByte >> 6];

      switch (fgBgTransp)
      {
        case 3:
          *pixels++ = (fgColor & mask1) | (bgColor & ~mask1);
          *pixels++ = (fgColor & mask2) | (bgColor & ~mask2);
          *pixels++ = (fgColor & mask3) | (bgColor & ~mask3);
          break;

        case 2:
          *pixels++ = (fgColor & mask1) | (*pixels & ~mask1);
          *pixels++ = (fgColor & mask2) | (*pixels & ~mask2);
          *pixels++ = (fgColor & mask3) | (*pixels & ~mask3);
          break;

        case 1:
          *pixels++ = (*pixels & mask1) | (bgColor & ~mask1);
          *pixels++ = (*pixels & mask2) | (bgColor & ~mask2);
          *pixels++ = (*pixels & mask3) | (bgColor & ~mask3);
          break;
      }
    }
  }
}


/* Function:  vrEmuTms9918Text80ScanLine
 * ----------------------------------------
 * generate an 80-column text mode scanline
 */
static void __time_critical_func(vrEmuTms9918Text80ScanLine)(VR_EMU_INST_ARG uint8_t y, uint8_t pixels[TMS9918_PIXELS_X])
{
  int originalY = y;
  bool swapYPage = false;
  if (TMS_REGISTER(tms9918, 0x1c))
  {
    int virtY = y;
    virtY += TMS_REGISTER(tms9918, 0x1c);

    int maxY = (TMS_REGISTER(tms9918, 0x31) & 0x40) ? (8 * 30) : (8 * 24);

    if (virtY >= maxY)
    {
      virtY -= maxY;
      swapYPage = (bool)(TMS_REGISTER(tms9918, 0x1d) & 0x01);
    }
   
    y = virtY;
  }

  uint8_t tileY = y >> 3;   /* which name table row (0 - 23... or 29) */
  uint8_t pattRow = y & 0x07;  /* which pattern row (0 - 7) */


  uint8_t nameTableMask = tms9918->isUnlocked ? 0x0f : 0x0c;  

  /* address in name table at the start of this row */
  uint32_t rowNamesAddr = (tmsNameTableAddr(tms9918) & (nameTableMask << 10)) + tileY * TEXT80_NUM_COLS;
  if (swapYPage) rowNamesAddr ^= 0x800;
  
  uint8_t* patternTable = tms9918->vram.bytes + tmsPatternTableAddr(tms9918) + pattRow;

  const vrEmuTms9918Color bgColor = tmsMainBgColor(tms9918);

  pixels += TEXT_PADDING_PX;

  dma_channel_wait_for_finish_blocking(dma32noinc); 

  if (TMS_REGISTER(tms9918, 0x32) & 0x02)  // position-based attributes
  {
    uint16_t colorTableAddr = (tmsColorTableAddr(tms9918)) + tileY * TEXT80_NUM_COLS;
    if (swapYPage) colorTableAddr ^= 0x800;

    const bool tilesDisabled = TMS_REGISTER(tms9918, 0x32) & 0x10;
    if (!tilesDisabled) renderText80Layer(y, tileY, tms9918->vram.bytes + rowNamesAddr, patternTable, tms9918->vram.bytes + colorTableAddr, true, pixels);

    if (TMS_REGISTER(tms9918, 0x31) & 0x80)
    {
      y = originalY;
      swapYPage = false;
      if (TMS_REGISTER(tms9918, 0x1a))
      {
        int virtY = y;
        virtY += TMS_REGISTER(tms9918, 0x1a);

        int maxY = (TMS_REGISTER(tms9918, 0x31) & 0x40) ? (8 * 30) : (8 * 24);

        if (virtY >= maxY)
        {
          virtY -= maxY;
          swapYPage = (bool)(TMS_REGISTER(tms9918, 0x1d) & 0x10);
        }
      
        y = virtY;
      }

      uint8_t tileY = y >> 3;   /* which name table row (0 - 23... or 29) */
      uint8_t pattRow = y & 0x07;  /* which pattern row (0 - 7) */

      const uint8_t startPattBit = TMS_REGISTER(tms9918, 0x1b) & 0x07;
      const uint8_t tileIndex = (TMS_REGISTER(tms9918, 0x1b) >> 3);

      uint8_t* patternTable = tms9918->vram.bytes + tmsPatternTableAddr(tms9918) + pattRow;

      rowNamesAddr = (tmsNameTable2Addr(tms9918) & (nameTableMask << 10)) + tileY * TEXT80_NUM_COLS;
      colorTableAddr = (tmsColorTable2Addr(tms9918) ) + tileY * TEXT80_NUM_COLS;
      if (swapYPage)
      {
        rowNamesAddr ^= 0x800;
        colorTableAddr ^= 0x800;
      }

      renderText80Layer(y, tileY, tms9918->vram.bytes + rowNamesAddr, patternTable, tms9918->vram.bytes + colorTableAddr, false, pixels);
    }
  }
  else  // just plain old two-tone
  {
    const vrEmuTms9918Color fgColor = tmsMainFgColor(tms9918);

    const uint32_t bgFgColor[4] =
    {
      (bgColor << 4) | bgColor,
      (bgColor << 4) | fgColor,
      (fgColor << 4) | bgColor,
      (fgColor << 4) | fgColor
    };

    uint8_t *rowNamesTable = tms9918->vram.bytes + rowNamesAddr;

    for (uint8_t tileX = 0; tileX < TEXT80_NUM_COLS; ++tileX)
    {
      uint8_t pattByte = patternTable[*rowNamesTable++ * PATTERN_BYTES];

      *pixels++ = bgFgColor[(pattByte >> 6) & 0x03];
      *pixels++ = bgFgColor[(pattByte >> 4) & 0x03];
      *pixels++ = bgFgColor[(pattByte >> 2) & 0x03];
    }
  }
}


/* Function:  renderEcmShiftedTile
 * ----------------------------------------
 * render the first shiny new ECM (enhanced color mode) graphics I tile in a scrolled scanline
 * this guy sets the stage for the remaining tiles (offset-wise). if the tile isn't scrolled
 * we end up just using renderEcmAlignedTile() instead
 *
 * quadPixels either incremented by 1 or 0, depending where it lands (how shifted it is)
 * 
 * INLINE: so will be different versions generated, depending on hard-coded (or known at compile-time) arguments
 */
static inline uint32_t* renderEcmStartTile(
  uint32_t *quadPixels,
  const uint32_t tilePixels[2],
  const uint32_t pattMask,
  const uint32_t startPattBit,
  const uint32_t shift)
{
  const uint32_t rightMask = maskExpandNibbleToWordRev[pattMask & 0xf];

  // first tile will either take one or two nibbles depending on the shift
  if (startPattBit < 4)
  {
    const uint32_t leftMask = maskExpandNibbleToWordRev[pattMask >> 4];
    const uint32_t reverseShift = 32 - shift;
    const uint32_t mask = (leftMask >> shift) | (rightMask << reverseShift);
    const uint32_t shifted = mask & ((tilePixels[0] >> shift) | (tilePixels[1] << reverseShift));
    *quadPixels++ = (*quadPixels & ~mask) | shifted;
  }

  const uint32_t mask = (rightMask >> shift);
  const uint32_t shifted = mask & (tilePixels[1] >> shift);
  *quadPixels = (*quadPixels & ~mask) | shifted;

  if (startPattBit == 4) ++quadPixels;

  return quadPixels;
}

/* Function:  renderEcmShiftedTile
 * ----------------------------------------
 * render a shiny new ECM (enhanced color mode) graphics I tile which is NOT aligned to a word boundary
 *
 * quadPixels always incremented by 2
 * 
 * INLINE: so will be different versions generated, depending on hard-coded (or known at compile-time) arguments
 */
static inline uint32_t* renderEcmShiftedTile(
  uint32_t *quadPixels,
  const uint32_t tilePixels[2],
  const uint32_t pattMask,
  const uint32_t shift,
  const uint32_t reverseShift) 
{
  const uint32_t rightMask = maskExpandNibbleToWordRev[pattMask & 0xf];
  const uint32_t leftMask = maskExpandNibbleToWordRev[pattMask >> 4];

  {
    const uint32_t mask = leftMask << reverseShift;
    const uint32_t shifted = mask & (tilePixels[0] << reverseShift);
    *quadPixels++ = (*quadPixels & ~mask) | shifted;
  }

  {
    uint32_t shifted = (tilePixels[0] >> shift) | (tilePixels[1] << reverseShift);
    const uint32_t mask = ~((leftMask >> shift) | (rightMask << reverseShift));
    if (mask) shifted = (*quadPixels & mask) | (~mask & shifted);
    *quadPixels++ = shifted;
  }

  {
    const uint32_t mask = (rightMask >> shift);
    const uint32_t shifted = mask & (tilePixels[1] >> shift);
    *quadPixels = (*quadPixels & ~mask) | shifted;
  }

  return quadPixels;
}

/* Function:  renderEcmAlignedTile
 * ----------------------------------------
 * render a shiny new ECM (enhanced color mode) graphics I tile which is aligned to a word boundary
 * 
 * quadPixels always incremented by 2
 * 
 * INLINE: so will be different versions generated, depending on hard-coded (or known at compile-time) arguments
 */
static inline uint32_t* renderEcmAlignedTile(
  uint32_t *quadPixels,
  const uint32_t tilePixels[2],
  const uint32_t pattMask)
{
  if (pattMask == 0xff)
  {
    *quadPixels++ = tilePixels[0];
    *quadPixels++ = tilePixels[1];
  }
  else
  {
    // not shifted, but transparent - need to mask the two nibbles
    const uint32_t rightMask = maskExpandNibbleToWordRev[pattMask & 0xf];
    const uint32_t leftMask = maskExpandNibbleToWordRev[pattMask >> 4];

    *quadPixels++ = (*quadPixels & ~leftMask) | (leftMask & tilePixels[0]);
    *quadPixels++ = (*quadPixels & ~rightMask) | (rightMask & tilePixels[1]);  
  }

  return quadPixels;
}

/* Function:  quadPixelIncrement
 * ----------------------------------------
 * compute the amount to increment our quad pixel pointer by.
 * generally, 2 words (8 pixel bytes), but in the case of the first tile
 * in a scrolled row, will either be 1 or even... 0 depending on how 
 * many pixels we're scrolled by
 */
static inline uint32_t quadPixelIncrement(uint32_t startPattBit)
{
  if (!startPattBit) return 2;
  return startPattBit <= 4;
}


/* Function:  renderEcm0Tile
 * ----------------------------------------
 * render an ECM0 (enhanced color mode) graphics I tile. basically the same as original, but can scroll
 * 
 * INLINE: so will be different versions generated, depending on hard-coded (or known at compile-time) arguments
 */
static inline uint32_t* renderEcm0Tile(
  uint32_t *quadPixels,
  const uint32_t xPos,
  const uint8_t pattIdx,
  const uint8_t patternTable[],
  const uint32_t colorTableAddr,
  const uint32_t startPattBit,
  const uint32_t pal,
  const uint32_t pattRow,
  const uint32_t shift,
  const bool isTile2)
{
  /* was this pattern empty? we remember the last empty pattern.
     OR is the pixel mask full here? in either case, let's bail */
  if ((!isTile2 && !tmsTestRowBitsMask(xPos, 0xff << (24 + startPattBit), 8, false, true, false)))
  {
    return quadPixels + quadPixelIncrement(startPattBit);
  }

  /* grab the attributes for this tile */
  uint32_t colorTableOffset = pattIdx >> 3;
  const uint32_t colorByte = tms9918->vram.bytes[colorTableAddr + colorTableOffset];

  uint32_t pattOffset = pattIdx * PATTERN_BYTES + pattRow;
  uint32_t leftIndex = 0, rightIndex = 0;

  const uint32_t patt = patternTable[pattOffset];

  const uint32_t bgColor = colorByte & 0x0f;
  const uint32_t fgColor = colorByte >> 4;

  const uint32_t bgPalette = repeatedPalette[pal | bgColor];
  const uint32_t fgPalette = repeatedPalette[pal | fgColor];

  const uint32_t rightMask = maskExpandNibbleToWordRev[patt & 0xf];
  const uint32_t leftMask = maskExpandNibbleToWordRev[patt >> 4];

  const uint32_t tilePixels[2] = {(fgPalette & leftMask) | (bgPalette & ~leftMask),
                                  (fgPalette & rightMask) | (bgPalette & ~rightMask)};

  /* have we any pixels to draw? */
  const uint32_t offset = (24 + startPattBit);  
  uint32_t pattMask = 0xff;
  if (!bgColor) pattMask &= patt;
  if (!fgColor) pattMask ^= patt;

  pattMask <<= offset;
  pattMask = tmsTestRowBitsMask(xPos - startPattBit, pattMask, 8, true, !isTile2, true);

    /* anything to draw?*/
  if (!pattMask)
  {
    return quadPixels + quadPixelIncrement(startPattBit);
  }

  pattMask >>= offset;

  if (startPattBit)
  {
    /* first tile gets different treatment because we discard the pixels shifted off the left */
    quadPixels = renderEcmStartTile(quadPixels, tilePixels, pattMask, startPattBit, shift << 3);
  }
  else
  {
    /* a regual shifted tile... we need to write three nibbles for these */
    switch (shift)
    {
      case 0:
        quadPixels = renderEcmAlignedTile(quadPixels, tilePixels, pattMask);
        break;
      case 1:
        quadPixels = renderEcmShiftedTile(quadPixels, tilePixels, pattMask, 8, 24);
        break;
      case 2:
        quadPixels = renderEcmShiftedTile(quadPixels, tilePixels, pattMask, 16, 16);
        break;
      default:
        quadPixels = renderEcmShiftedTile(quadPixels, tilePixels, pattMask, 24, 8);
        break;
    }
  }

  return quadPixels;
}


/* Function:  renderEcmTile
 * ----------------------------------------
 * render a shiny new ECM (enhanced color mode) graphics I tile
 * 
 * INLINE: so will be different versions generated, depending on hard-coded (or known at compile-time) arguments
 */
static inline uint32_t* renderEcmTile(
  uint32_t *quadPixels,
  const uint32_t xPos,
  const uint8_t pattIdx,
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
  uint32_t *lastEmpty,
  const bool isTile2,
  const bool alwaysOnTop)
{
  /* was this pattern empty? we remember the last empty pattern.
     OR is the pixel mask full here? in either case, let's bail */
  if (*lastEmpty == pattIdx ||
      (!isTile2 && !tmsTestRowBitsMask(xPos, 0xff << (24 + startPattBit), 8, false, true, false)))
  {
    return quadPixels + quadPixelIncrement(startPattBit);
  }

  /* grab the attributes for this tile */
  uint32_t colorTableOffset = attrPerPos ? tileIndex : pattIdx;
  uint32_t pattOffset = pattIdx * PATTERN_BYTES;
  
  const uint32_t colorByte = tms9918->vram.bytes[colorTableAddr + colorTableOffset];

  pattOffset += (colorByte & 0x20) ? 7 - pattRow : pattRow;

  uint32_t pattMask = (colorByte & 0x10) ? 0 : 0xff;
  uint32_t leftIndex = 0, rightIndex = 0;

  /* retreive the pixel data for each ecm bitplane, and generate a
     combined mask while we're at it. if the mask has a bit set
     then we have a non-zero pixel at that location */
  uint32_t patt[3]; // indexes into this are reversed. ecm3 is in index 0


  switch (ecm)
  {
    case 3:
      patt[0] = patternTable[pattOffset + ecmOffset * 2];
    case 2:
      patt[1] = patternTable[pattOffset + ecmOffset];
    default:
      patt[2] = patternTable[pattOffset];
  }

  if (colorByte & 0x40) // flipX
  {
    patt[0] = reversedBits[patt[0]];
    patt[1] = reversedBits[patt[1]];
    patt[2] = reversedBits[patt[2]];
  }

  switch (ecm)
  {
    case 3:
        pattMask |= patt[0];
        leftIndex = (patt[0] >> 4) << 8;
        rightIndex = (patt[0] & 0xf) << 8;
    case 2:
        pattMask |= patt[1];
        leftIndex |= (patt[1] & 0xf0);
        rightIndex |= (patt[1] & 0xf) << 4;
    default:
        pattMask |= patt[2];
        leftIndex |= (patt[2] >> 4);
        rightIndex |= (patt[2] & 0xf);
  }

  /* have we any pixels to draw? */
  if (pattMask)
  {
    const uint32_t priority = alwaysOnTop || (colorByte & 0x80);
    const uint32_t offset = (24 + startPattBit);
    pattMask <<= offset;
    if (!priority)
      pattMask = tmsTestRowBitsMask(xPos - startPattBit, pattMask, 8, true, !isTile2, true);
    else
      pattMask = tmsTestRowBitsMask(xPos - startPattBit, pattMask, 8, true, !isTile2, false);

      /* anything to draw?*/
    if (!pattMask)
    {
      /* we don't set lastEmpty here, because it had pixels.. they were just masked out */
      return quadPixels + quadPixelIncrement(startPattBit);
    }

    pattMask >>= offset;

    const uint32_t palette = repeatedPalette[pal | ((colorByte & ecmColorMask) << ecmColorOffset)];
    const uint32_t tilePixels[2] = {ecmLookup[leftIndex] | palette,
                                    ecmLookup[rightIndex] | palette};

    if (startPattBit)
    {
      /* first tile gets different treatment because we discard the pixels shifted off the left */
      quadPixels = renderEcmStartTile(quadPixels, tilePixels, pattMask, startPattBit, shift << 3);
    }
    else
    {
      /* a regular shifted tile... we need to write three nibbles for these */
      switch (shift)
      {
        case 0:
            /* not shifted, but has transparency. we'll need to mask it */
          quadPixels = renderEcmAlignedTile(quadPixels, tilePixels, pattMask);
          break;
        case 1:
          quadPixels = renderEcmShiftedTile(quadPixels, tilePixels, pattMask, 8, 24);
          break;
        case 2:
          quadPixels = renderEcmShiftedTile(quadPixels, tilePixels, pattMask, 16, 16);
          break;
        case 3:
          quadPixels = renderEcmShiftedTile(quadPixels, tilePixels, pattMask, 24, 8);
          break;
      }
    }
  }
  else
  {
    quadPixels += quadPixelIncrement(startPattBit);
    *lastEmpty = pattIdx;
  }
  return quadPixels;
}

static const uint32_t zeroPixels[2] = {0, 0};

/* Function:  writeToAlignedBuffer
 * ----------------------------------------
 * Write full tile to aligned buffer - 8 pixels at once
 */
static inline void writeToAlignedBuffer(uint8_t* buffer, uint32_t xPos, const uint32_t tilePixels[2])
{
  uint32_t* buffer_words = (uint32_t*)(buffer + xPos);
  buffer_words[0] = tilePixels[0];
  buffer_words[1] = tilePixels[1];
}

/* Function:  renderEcmTileToAlignedBuffer
 * ----------------------------------------
 * render ECM tile to aligned buffer with all existing optimizations preserved
 */
static inline void renderEcmTileToAlignedBuffer(
  VR_EMU_INST_ARG uint8_t* buffer,
  const uint32_t xPos,
  const uint8_t pattIdx,
  const uint8_t patternTable[],
  const uint32_t colorTableAddr,
  const uint32_t ecm,
  const uint32_t ecmOffset,
  const uint32_t ecmColorMask,
  const uint32_t ecmColorOffset,
  const uint32_t pal,
  const bool attrPerPos,
  const uint32_t rowOffset,
  const uint32_t pattRow,
  const uint32_t tileIndex,
  uint32_t *lastEmpty,
  const bool isTile2,
  const bool alwaysOnTop)
{
  /* was this pattern empty? we remember the last empty pattern.
     OR is the pixel mask full here? in either case, let's bail */
  if ((*lastEmpty == pattIdx) ||
      (!isTile2 && !tmsTestRowBitsMaskAligned(xPos, 0xff << 24, tms9918->finalMask)))
  {
    // For T1, write transparent pixels (0) instead of skipping
    if (!isTile2)
    {
      writeToAlignedBuffer(buffer, xPos, zeroPixels);
    }
    return;
  }

  /* grab the attributes for this tile */
  uint32_t colorTableOffset = attrPerPos ? tileIndex : pattIdx;
  uint32_t pattOffset = pattIdx * PATTERN_BYTES;
  
  const uint32_t colorByte = tms9918->vram.bytes[colorTableAddr + colorTableOffset];

  pattOffset += (colorByte & 0x20) ? 7 - pattRow : pattRow;

  uint32_t pattMask = (colorByte & 0x10) ? 0 : 0xff;
  uint32_t leftIndex = 0, rightIndex = 0;

  /* retreive the pixel data for each ecm bitplane, and generate a
     combined mask while we're at it. if the mask has a bit set
     then we have a non-zero pixel at that location */
  uint8_t patt[3]; // indexes into this are reversed. ecm3 is in index 0

  switch (ecm)
  {
    case 3:
      patt[0] = patternTable[pattOffset + ecmOffset * 2];
      pattMask |= patt[0];
    case 2:
      patt[1] = patternTable[pattOffset + ecmOffset];
      pattMask |= patt[1];
    default:
      patt[2] = patternTable[pattOffset];
      pattMask |= patt[2];
  }

  /* have we any pixels to draw? */
  if (pattMask)
  {
    if (colorByte & 0x40) // flipX
    {
      patt[0] = reversedBits[patt[0]];
      patt[1] = reversedBits[patt[1]];
      patt[2] = reversedBits[patt[2]];
      pattMask = reversedBits[pattMask];
    }


    //if (!priority)
    //  pattMask = tmsTestRowBitsMask(xPos - startPattBit, pattMask, 8, true, !isTile2, true);
    //else
    //  pattMask = tmsTestRowBitsMask(xPos - startPattBit, pattMask, 8, true, !isTile2, false);
      /* anything to draw?*/
    if (!pattMask)
    {
      /* we don't set lastEmpty here, because it had pixels.. they were just masked out */
      // For T1, write transparent pixels (0) instead of skipping
      if (!isTile2)
      {
        writeToAlignedBuffer(buffer, xPos, zeroPixels);
      }
      return;
    }
    const uint32_t priority = alwaysOnTop || (colorByte & 0x80);
    const uint32_t offset = 24;
    pattMask <<= 24;

//    pattMask >>= offset;
    if (isTile2) tmsUpdateRowBitsMaskAligned(xPos, pattMask, tms9918->layerSelectionMask);

    switch (ecm)
    {
      case 3:
          rightIndex = (patt[0] & 0xf) << 8;
          leftIndex = (patt[0] >> 4) << 8;
      case 2:
          rightIndex |= (patt[1] & 0xf) << 4;
          leftIndex |= (patt[1] & 0xf0);
      default:
          rightIndex |= (patt[2] & 0xf);
          leftIndex |= (patt[2] >> 4);
    }

    const uint32_t palette = repeatedPalette[pal | ((colorByte & ecmColorMask) << ecmColorOffset)];
    const uint32_t tilePixels[2] = {ecmLookup[leftIndex] | palette,
                                    ecmLookup[rightIndex] | palette};

    // Write to aligned buffer instead of doing expensive bit shifting
    writeToAlignedBuffer(buffer, xPos, tilePixels);
  }
  else
  {
    // For T1, write transparent pixels (0) even when tile is empty
    if (!isTile2)
    {
      writeToAlignedBuffer(buffer, xPos, zeroPixels);
    }
    *lastEmpty = pattIdx;
  }
}

/* Function:  vrEmuF18ATileScanLine
 * ----------------------------------------
 * generate an F18A tile layer scanline
 * 
 * INLINE: so will be different versions generated, depending on hard-coded (or known at compile-time) arguments (T1 or T2)
 */
static inline void __time_critical_func(vrEmuF18ATileScanLine)(VR_EMU_INST_ARG const uint8_t y, const bool hpSize, uint16_t rowNamesAddr, uint16_t colorTableAddr, const uint16_t rowOffset, uint8_t tileIndex, uint8_t startPattBit, const bool attrPerPos, uint8_t pal, const bool alwaysOnTop, const bool isTile2, uint8_t pixels[TMS9918_PIXELS_X])
{
  uint32_t xPos = 0;
  uint32_t lastEmpty = -1;
 
  const uint32_t pattRow = y & 0x07;  /* which pattern row (0 - 7) */
  const uint8_t* patternTable = tms9918->vram.bytes + tmsPatternTableAddr(tms9918);

  // for the entire scanline, we need to shift our 4-pixel words by this much
  uint32_t lastPattId = -1;

  /* iterate over each tile in this row - if' we're scrolling, add one */
  uint32_t numTiles = GRAPHICS_NUM_COLS + 1;
  //if (startPattBit != 0) numTiles++;  // Add extra tile for scrolling

  /* keep in mind when using this... the byte order will be reversed */
  uint32_t *quadPixels = (uint32_t*)pixels;

  if (tms9918->isUnlocked)
  {
    const uint32_t ecm = (TMS_REGISTER(tms9918, 0x31) & 0x30) >> 4;
    
    if (ecm)
    {
      const uint32_t ecmColorOffset = (ecm == 3) ? 2 : ecm;
      const uint32_t ecmColorMask = (ecm == 3) ? 0x0e : 0x0f;
      const uint32_t ecmOffset = 0x800 >> ((TMS_REGISTER(tms9918, 0x1d) & 0x0c) >> 2);
      
      if (ecm == 1)
      {
        pal &= 0x20;
      }
      else
      {
        pal = 0;
      }

      dma_channel_wait_for_finish_blocking(dma32noinc); 

      // Optimization: use aligned buffers when any horizontal scrolling is active
     // if (shift)// || (TMS_REGISTER(tms9918, 0x1b) & 0x07) || (TMS_REGISTER(tms9918, 0x19) & 0x07))
      {
        // Get the appropriate aligned buffer for this layer
        uint8_t* targetBuffer = isTile2 ? tms9918->tileLayer2Buffer : tms9918->tileLayer1Buffer;
        

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
          const uint8_t pattIdx = tms9918->vram.bytes[rowNamesAddr + tileIndex];
          renderEcmTileToAlignedBuffer(VR_EMU_INST targetBuffer, xPos, pattIdx, patternTable, colorTableAddr, ecm, ecmOffset,
                                      ecmColorMask, ecmColorOffset, pal, attrPerPos, rowOffset, pattRow, tileIndex++, 
                                      &lastEmpty, isTile2, alwaysOnTop);
          xPos += 8;
        }
        
        // Early exit - don't use original bit-shifting code
        return;
      }
    }
    else  // ECM0 is a bit different
    {
      if (startPattBit) ++numTiles;
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
        const uint8_t pattIdx = tms9918->vram.bytes[rowNamesAddr + tileIndex];
        quadPixels = renderEcm0Tile(quadPixels, xPos, pattIdx, patternTable, colorTableAddr, startPattBit, pal, pattRow, 0, isTile2);
        xPos += 8 - startPattBit;
        startPattBit = 0;
        ++tileIndex;
      }
    }
  }
  else
  {
    dma_channel_wait_for_finish_blocking(dma32noinc); 

    uint16_t* twoPixels = (uint16_t*)pixels;
    uint8_t lastColorByte = 0;
    uint16_t fgbg[4] = {0};
    uint8_t *pattPtr = tms9918->vram.bytes + rowNamesAddr + tileIndex;
    uint8_t lastPattIdx = 0;
    uint32_t pattOffset = 0;
    const uint8_t *pattTableRow = patternTable + pattRow;
    int8_t pattByte = pattTableRow[0];

    while (numTiles--)
    {
      const uint8_t pattIdx = *pattPtr++;

      if (lastPattIdx != pattIdx)
      {
        lastPattIdx = pattIdx;
        pattOffset = lastPattIdx * PATTERN_BYTES;
        pattByte = pattTableRow[pattOffset];
        const uint32_t colorByte = tms9918->vram.bytes[colorTableAddr + (pattIdx >> 3)];
        if (lastColorByte != colorByte)
        {
          lastColorByte = colorByte;

          // Get fg/bg colors
          uint8_t bg = pal | tmsBgColor(tms9918, colorByte);
          uint8_t fg = pal | tmsFgColor(tms9918, colorByte);

          // Generate 2-bit LUT: 4 combinations of 2 pixels each (packed as uint16_t)
          fgbg[0] = bg | (bg << 8);        // 00: bg,bg
          fgbg[1] = bg | (fg << 8);        // 01: bg,fg
          fgbg[2] = fg | (bg << 8);        // 10: fg,bg
          fgbg[3] = fg | (fg << 8);        // 11: fg,fg
        }
      }

      *twoPixels++ = fgbg[(pattByte >> 6) & 3];  // Pixels 0,1
      *twoPixels++ = fgbg[(pattByte >> 4) & 3];  // Pixels 2,3
      *twoPixels++ = fgbg[(pattByte >> 2) & 3];  // Pixels 4,5
      *twoPixels++ = fgbg[pattByte & 3];         // Pixels 6,7

      ++tileIndex;
      xPos += 8;
    }
  }  
}

typedef struct {
  uint8_t vertScrollReg;
  uint8_t yPageSwapMask;
  uint8_t paletteShift;
  uint8_t paletteMask;
  uint8_t startPattReg;
  uint8_t hpSizeMask;
  uint8_t priorityReg;
  uint8_t priorityMask;
  bool isTile2;
  uint16_t (*nameTableAddrFunc)(VrEmuTms9918*);
  uint16_t (*colorTableAddrFunc)(VrEmuTms9918*);
  bool useOrLogicForColorTable;
} TileLayerConfig;

static const TileLayerConfig T1_CONFIG = {
  .vertScrollReg = 0x1c,
  .yPageSwapMask = 0x01,
  .paletteShift = 4,
  .paletteMask = 0x03,
  .startPattReg = 0x1b,
  .hpSizeMask = 0x02,
  .priorityReg = 0,        // T1 has no priority control
  .priorityMask = 0,
  .isTile2 = false,
  .nameTableAddrFunc = tmsNameTableAddr,
  .colorTableAddrFunc = tmsColorTableAddr,
  .useOrLogicForColorTable = false
};

static const TileLayerConfig T2_CONFIG = {
  .vertScrollReg = 0x1a,
  .yPageSwapMask = 0x10,
  .paletteShift = 2,
  .paletteMask = 0x0c,
  .startPattReg = 0x19,
  .hpSizeMask = 0x20,
  .priorityReg = 0x32,
  .priorityMask = 0x01,
  .isTile2 = true,
  .nameTableAddrFunc = tmsNameTable2Addr,
  .colorTableAddrFunc = tmsColorTable2Addr,
  .useOrLogicForColorTable = true
};

/* Function:  vrEmuF18ATileLayerScanLine
  * ----------------------------------------
  * generate a Graphics I mode scanline for either T1 or T2 layer
  */
static void __time_critical_func(vrEmuF18ATileLayerScanLine)(
  VR_EMU_INST_ARG
  uint8_t y,
  uint8_t pixels[TMS9918_PIXELS_X],
  const TileLayerConfig* config)
{
  bool swapYPage = false;

  /* vertical scroll */
  if (TMS_REGISTER(tms9918, config->vertScrollReg))
  {
    int virtY = y;
    virtY += TMS_REGISTER(tms9918, config->vertScrollReg);

    int maxY = (TMS_REGISTER(tms9918, 0x31) & 0x40) ? (8 * 30) : (8 * 24);

    if (virtY >= maxY)
    {
      virtY -= maxY;
      swapYPage = TMS_REGISTER(tms9918, 0x1d) & config->yPageSwapMask;
    }

    y = virtY;
  }

  const uint8_t tileY = y >> 3;   /* which name table row (0 - 23)... or 29 */

  /* address in name table at the start of this row */
  const bool attrPerPos = TMS_REGISTER(tms9918, 0x32) & 0x02;
  const uint16_t rowOffset = tileY * GRAPHICS_NUM_COLS;

  uint16_t rowNamesAddr = config->nameTableAddrFunc(tms9918) + rowOffset;
  if (swapYPage) rowNamesAddr ^= 0x800;

  uint16_t colorTableAddr = config->colorTableAddrFunc(tms9918);
  if (attrPerPos)
  {
    colorTableAddr += rowOffset;
    if (config->useOrLogicForColorTable)
    {
      colorTableAddr |= rowNamesAddr & 0xc00;
    }
    else if (swapYPage)
    {
      colorTableAddr ^= 0x800;
    }
  }

  const uint8_t pal = (TMS_REGISTER(tms9918, 0x18) & config->paletteMask) << config->paletteShift;
  const uint8_t startPattBit = TMS_REGISTER(tms9918, config->startPattReg) & 0x07;
  const uint8_t tileIndex = (TMS_REGISTER(tms9918, config->startPattReg) >> 3);
  const bool hpSize = TMS_REGISTER(tms9918, 0x1d) & config->hpSizeMask;

  const bool alwaysOnTop = config->priorityReg ?
    !(TMS_REGISTER(tms9918, config->priorityReg) & config->priorityMask) : false;

  vrEmuF18ATileScanLine(VR_EMU_INST y, hpSize, rowNamesAddr, colorTableAddr, rowOffset,
                        tileIndex, startPattBit, attrPerPos, pal, alwaysOnTop, config->isTile2, pixels);
}

/* Wrapper functions */
static void __time_critical_func(vrEmuF18ATile1ScanLine)(VR_EMU_INST_ARG uint8_t y, uint8_t
pixels[TMS9918_PIXELS_X])
{
  vrEmuF18ATileLayerScanLine(VR_EMU_INST y, pixels, &T1_CONFIG);
}

static void __time_critical_func(vrEmuF18ATile2ScanLine)(VR_EMU_INST_ARG uint8_t y, uint8_t
pixels[TMS9918_PIXELS_X])
{
  vrEmuF18ATileLayerScanLine(VR_EMU_INST y, pixels, &T2_CONFIG);
}

/* Function:  renderBitmapLayer
 * ----------------------------------------
 * generate an F18A bitmap layer scanline
 * 
 * INLINE: so will be different versions generated, depending on hard-coded (or known at compile-time) arguments
 */
static inline bool __time_critical_func(renderBitmapLayer)(VR_EMU_INST_ARG uint8_t y, bool opaque, const uint8_t width, const uint16_t addr, const uint8_t bmlCtl, uint8_t pixels[TMS9918_PIXELS_X])
{
  bool writeMask = bmlCtl & 0x40;

  bool returnVal = true;

  if (writeMask && opaque && (width == 64))
  {
    for (int i = 0; i < TMS9918_PIXELS_X / 32; ++i)
      rowBits[i] = -1;
    writeMask = false;
    returnVal = false;
  }
  
  uint32_t currentMask = 0;
  uint8_t xPos = TMS_REGISTER(tms9918, 0x21);
  
  if (bmlCtl & 0x10)  // fat 4bpp pixels?
  {
    const uint8_t colorMask = 0xf0;
    const uint8_t colorOffset = 4;
    const uint8_t colorCount = 2;
    const uint8_t colorSize = 4;
    uint32_t maskPixelMask = 0x3 << 30;
    uint32_t maskX = xPos;

    uint8_t pal = (bmlCtl & 0xc) << 2;
    
    dma_channel_wait_for_finish_blocking(dma32noinc); 

    for (int xOff = 0; xOff < width; ++xOff)
    {
      uint8_t data = tms9918->vram.bytes[addr + xOff];
      for (int sp = 0; sp < colorCount; ++sp)
      {
        uint8_t color = (data & colorMask);
        if (opaque || color)
        {
          uint8_t finalColour = pal | (color >> colorOffset);
          pixels[xPos] = finalColour;
          pixels[xPos + 1] = finalColour;
          currentMask |= maskPixelMask;
        }
        xPos += 2;
        data <<= colorSize;
        maskPixelMask >>= 2;
      }
      if (writeMask && !maskPixelMask && currentMask)
      {
        tmsTestRowBitsMask(maskX, currentMask, 32, true, false, false);
        maskX = xPos;
        maskPixelMask = 0x3 << 30;
        currentMask = 0;
      }
    }
    if (writeMask && currentMask)
    {
      tmsTestRowBitsMask(maskX, currentMask, xPos - maskX, true, false, false);
    }
  }
  else // regular 2bpp pixels
  {
    const uint8_t colorMask = 0xc0;
    const uint8_t colorOffset = 6;
    const uint8_t colorCount = 4;
    const uint8_t colorSize = 2;
    uint32_t maskPixelMask = 0x1 << 31;
    uint32_t maskX = xPos;

    uint8_t pal = (bmlCtl & 0xf) << 2;

    dma_channel_wait_for_finish_blocking(dma32noinc); 

    for (int xOff = 0; xOff < width; ++xOff)
    {
      uint8_t data = tms9918->vram.bytes[addr + xOff];
      for (int sp = 0; sp < colorCount; ++sp)
      {
        uint8_t color = (data & colorMask);
        if (opaque || color)
        {
          pixels[xPos] = pal | (color >> colorOffset);
          currentMask |= maskPixelMask;
        }
        ++xPos;
        data <<= colorSize;
        maskPixelMask >>= 1;
      }

      if (writeMask && !maskPixelMask && currentMask)
      {
        tmsTestRowBitsMask(maskX, currentMask, 32, true, false, false);
        maskX = xPos;
        maskPixelMask = 0x1 << 31;
        currentMask = 0;
      }
    }
    if (writeMask && currentMask)
    {
      tmsTestRowBitsMask(maskX, currentMask, xPos - maskX, true, false, false);
    }
  }
  return returnVal;
}


/* Function:  vrEmuTms9918BitmapLayerScanLine
 * ----------------------------------------
 * generate an F18A bitmap layer scanline
 */
static bool __time_critical_func(vrEmuTms9918BitmapLayerScanLine)(VR_EMU_INST_ARG uint8_t y, uint8_t pixels[TMS9918_PIXELS_X])
{
  /* bml enabled? */
  const uint8_t bmlCtl = TMS_REGISTER(tms9918, 0x1f);
  if (!(bmlCtl & 0x80))
    return true;

  /* bml on this scanline? */
  const uint8_t top = TMS_REGISTER(tms9918, 0x22);
  if (top > y)
    return true;

  y -= top;
  if (y >= TMS_REGISTER(tms9918, 0x24))
    return true;

  const uint8_t width = TMS_REGISTER(tms9918, 0x23) ? (TMS_REGISTER(tms9918, 0x23) >> 2) : 64;
  const uint16_t addr = (TMS_REGISTER(tms9918, 0x20) << 6) + (y * width);

  //if (bmlCtl & 0x20) // transp
  {
    return renderBitmapLayer(VR_EMU_INST y, !(bmlCtl & 0x20), width, addr, bmlCtl, pixels);
  }
//  else
  {
//    renderBitmapLayer(VR_EMU_INST y, false, width, addr, bmlCtl, pixels);
  }
}

/* Function:  compositeAlignedTileBuffersWithDMA
 * ----------------------------------------
 * Use DMA for efficient tile buffer compositing
 */
static inline void compositeAlignedTileBuffersWithDMA(VR_EMU_INST_ARG uint8_t pixels[TMS9918_PIXELS_X], const int t1Scroll, const int t2Scroll)
{
  uint8_t* layer1 = tms9918->tileLayer1Buffer + t1Scroll;
  uint8_t* layer2 = tms9918->tileLayer2Buffer + t2Scroll;
  uint32_t* selectionMask = tms9918->layerSelectionMask;
  
  // For regions with only one layer active, use DMA copy
  // For mixed regions, use CPU compositing
  
  uint32_t processedPixels = 0;
  
  // Process in 32-pixel chunks (1 mask word at a time)
  for (int maskWord = 0; maskWord < TMS9918_PIXELS_X / 32; maskWord++)
  {
    const uint32_t mask = selectionMask[maskWord];
    const uint32_t pixelStart = maskWord * 32;
    
    if (mask == 0)
    {
      // All T1 pixels - use DMA copy for speed  
      dma_channel_wait_for_finish_blocking(dma32inc);
      dma_channel_set_read_addr(dma32inc, &layer1[pixelStart], false);
      dma_channel_set_write_addr(dma32inc, &pixels[pixelStart], false);
      dma_channel_set_trans_count(dma32inc, 32, true);  // 32 pixels = 8 words
    }
    else if (mask == 0xFFFFFFFF)
    {
      // All T2 pixels - use DMA copy for speed
      dma_channel_wait_for_finish_blocking(dma32inc);
      dma_channel_set_read_addr(dma32inc, &layer2[pixelStart], false);
      dma_channel_set_write_addr(dma32inc, &pixels[pixelStart], false);
      dma_channel_set_trans_count(dma32inc, 32, true);  // 32 pixels = 8 words
    }
    else
    {
      // Mixed - use CPU compositing for this 32-pixel region
      uint32_t pixelIndex = pixelStart;
      for (int i = 31; i >= 0; --i, ++pixelIndex)
      {
        const uint32_t maskBit = 1U << i;
        
        if (mask & maskBit)
          pixels[pixelIndex] = layer2[pixelIndex];  // T1 pixel
        else
          pixels[pixelIndex] = layer1[pixelIndex];  // T2 pixel
      }
    }
  }
  
  dma_channel_wait_for_finish_blocking(dma32noinc);
}



/* Function:  vrEmuTms9918GraphicsIScanLine
 * ----------------------------------------
 * generate a Graphics I mode scanline
 */
static uint8_t __time_critical_func(vrEmuTms9918GraphicsIScanLine)(VR_EMU_INST_ARG uint8_t y, uint8_t pixels[TMS9918_PIXELS_X])
{
  uint8_t tempStatus = 0;

  if (tms9918->isUnlocked)
  {
    bool writeMask = vrEmuTms9918BitmapLayerScanLine(VR_EMU_INST y, pixels);

    tempStatus = vrEmuTms9918OutputSprites(VR_EMU_INST y, pixels);

    if (writeMask)
    {
      // Check if horizontal scrolling is active on either layer
      const int t1Scroll = TMS_REGISTER(tms9918, 0x1b) & 0x07;
      const int t2Scroll = TMS_REGISTER(tms9918, 0x19) & 0x07;
      const bool t1HasScroll = t1Scroll != 0;
      const bool t2HasScroll = t2Scroll != 0;
      const bool useOptimization = true;//t1HasScroll || t2HasScroll;
      
      if (TMS_REGISTER(tms9918, 0x31) & 0x80)
      {
        vrEmuF18ATile2ScanLine(y, tms9918->tileLayer2Buffer);
        tmsCopyAlignMask(tms9918->finalMask, tms9918->layerSelectionMask, t1Scroll - t2Scroll);
      }

      if (!(TMS_REGISTER(tms9918, 0x32) & 0x10))
      {
        vrEmuF18ATile1ScanLine(y, tms9918->tileLayer1Buffer);
        tmsCopyAlignMask(tms9918->layerSelectionMask, tms9918->finalMask, -t1Scroll);
      }

      if (useOptimization)
      {
        compositeAlignedTileBuffersWithDMA(VR_EMU_INST pixels, t1Scroll, t2Scroll);
      }
    }
  }
  else
  {
    const uint8_t tileY = y >> 3;   /* which name table row (0 - 23)... or 29 */

    /* address in name table at the start of this row */
    const uint16_t rowOffset = tileY * GRAPHICS_NUM_COLS;
    uint16_t rowNamesAddr = tmsNameTableAddr(tms9918) + rowOffset;
    uint16_t colorTableAddr = tmsColorTableAddr(tms9918);

    const bool attrPerPos = false;
    const uint8_t pal = 0;
    const uint8_t startPattBit = 0;
    const uint8_t tileIndex = 0;
    const bool hpSize = 0;
    const bool isTile2 = false;

    dma_channel_wait_for_finish_blocking(dma32noinc); 

    vrEmuF18ATileScanLine(VR_EMU_INST y, hpSize, rowNamesAddr, colorTableAddr, rowOffset, tileIndex, startPattBit, attrPerPos, pal, isTile2, 0, pixels);

    tempStatus = vrEmuTms9918OutputSprites(VR_EMU_INST y, pixels);
  }

  return tempStatus;
}

/* Function:  vrEmuTms9918GraphicsIIScanLine
 * ----------------------------------------
 * generate a Graphics II mode scanline
 */
static  __attribute__((noinline)) void __time_critical_func(vrEmuTms9918GraphicsIIScanLine)(VR_EMU_INST_ARG uint8_t y, uint8_t pixels[TMS9918_PIXELS_X])
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
  const uint8_t nameMask = ((TMS_REGISTER(tms9918, TMS_REG_COLOR_TABLE) & 0x7f) << 3) | 0x07;

  const uint16_t pageThird = ((tileY & 0x18) >> 3)
    & (TMS_REGISTER(tms9918, TMS_REG_PATTERN_TABLE) & 0x03); /* which page? 0-2 */
  const uint16_t pageOffset = pageThird << 11; /* offset (0, 0x800 or 0x1000) */

  const uint8_t* patternTable = tms9918->vram.bytes + tmsPatternTableAddr(tms9918) + pageOffset + pattRow;
  const uint8_t* colorTable = tms9918->vram.bytes + tmsColorTableAddr(tms9918) + (pageOffset
    & ((TMS_REGISTER(tms9918, TMS_REG_COLOR_TABLE) & 0x60) << 6)) + pattRow;

  uint8_t palette = (TMS_REGISTER(tms9918, 0x18) & 0x03) << 4;

  dma_channel_wait_for_finish_blocking(dma32noinc); 

  /* iterate over each tile in this row */
  for (uint8_t tileX = 0; tileX < GRAPHICS_NUM_COLS; ++tileX)
  {
    uint8_t pattIdx = tms9918->vram.bytes[rowNamesAddr + tileX] & nameMask;

    const size_t pattRowOffset = pattIdx * PATTERN_BYTES;
    int8_t pattByte = patternTable[pattRowOffset];
    const uint8_t colorByte = colorTable[pattRowOffset];    
    
    // apply F18A palette. TODO put behind unlocked
    uint8_t bgColor = colorByte & 0x0f;
    uint8_t fgColor = colorByte >> 4;

    if (bgColor) {bgColor |= palette;} else {bgColor = tmsMainBgColor(tms9918);}
    if (fgColor) {fgColor |= palette;} else {fgColor = tmsMainBgColor(tms9918);}

    const uint32_t bgFgColor[] = {
      bgColor,
      fgColor
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

  const uint8_t* nameTable = tms9918->vram.bytes + tmsNameTableAddr(tms9918) + tileY * GRAPHICS_NUM_COLS;
  const uint8_t* patternTable = tms9918->vram.bytes + tmsPatternTableAddr(tms9918) + pattRow;

  uint32_t *quadPixels = (uint32_t *)pixels;

  dma_channel_wait_for_finish_blocking(dma32noinc); 

  for (uint8_t tileX = 0; tileX < GRAPHICS_NUM_COLS; ++tileX)
  {
    const uint8_t colorByte = patternTable[nameTable[tileX] * PATTERN_BYTES];
    const uint32_t fgColor = repeatedPalette[tmsFgColor(tms9918, colorByte)];
    const uint32_t bgColor = repeatedPalette[tmsBgColor(tms9918, colorByte)];

    *quadPixels++ = fgColor;
    *quadPixels++ = bgColor;
  }
}
	
/* Function:  vrEmuTms9918ScanLine
 * ----------------------------------------
 * generate a scanline
 */
VR_EMU_TMS9918_DLLEXPORT uint8_t __time_critical_func(vrEmuTms9918ScanLine)(VR_EMU_INST_ARG uint8_t y, uint8_t pixels[TMS9918_PIXELS_X])
{
  uint8_t tempStatus = 0;

  if (!lookupsReady)
  {
    initLookups();
    dma_channel_config cfg = dma_channel_get_default_config(dma32noinc);
	  channel_config_set_read_increment(&cfg, false);
	  channel_config_set_write_increment(&cfg, true);
	  channel_config_set_transfer_data_size(&cfg, DMA_SIZE_32);
	  dma_channel_set_config(dma32noinc, &cfg, false);
    dma_channel_set_read_addr(dma32noinc, &bg, false);
    dma_channel_set_trans_count(dma32noinc, TMS9918_PIXELS_X / 4, false);

    cfg = dma_channel_get_default_config(dma32inc);
	  channel_config_set_write_increment(&cfg, true);
	  channel_config_set_transfer_data_size(&cfg, DMA_SIZE_8);
	  dma_channel_set_config(dma32inc, &cfg, false);
  }

  tmsCachedMode = tmsMode(tms9918);

  /* clear the buffer with background color */
  //bg = repeatedPalette[tmsMainBgColor(tms9918)];
  //if (tmsCachedMode == TMS_MODE_TEXT80) bg |= bg << 4;
  //dma_channel_set_write_addr(dma32noinc, pixels, true);

  bool dispActive = (TMS_REGISTER(tms9918, TMS_REG_1) & TMS_R1_DISP_ACTIVE);

  if (dispActive)
  {
    /* use rowSpriteBits as a mask when tiles have priority over sprites */
    for (int i = 0; i < 9; ++i)
    {
      rowSpriteBits[i] = 0;
      rowBits[i] = 0;
      tms9918->layerSelectionMask[i] = 0; // Default to all T1 pixels
    }
    tms9918->scanlineHasSprites = false;

    switch (tmsCachedMode)
    {
      case TMS_MODE_GRAPHICS_I:
        tempStatus = vrEmuTms9918GraphicsIScanLine(VR_EMU_INST y, pixels);
        break;

      case TMS_MODE_GRAPHICS_II:
        vrEmuTms9918GraphicsIIScanLine(VR_EMU_INST y, pixels);
        tempStatus = vrEmuTms9918OutputSprites(VR_EMU_INST y, pixels);

        if (tms9918->isUnlocked)
        {
         // const bool tilesDisabled = TMS_REGISTER(tms9918, 0x32) & 0x10;

          vrEmuTms9918BitmapLayerScanLine(VR_EMU_INST y, pixels);
        }

        break;

      case TMS_MODE_TEXT:
        vrEmuTms9918TextScanLine(VR_EMU_INST y, pixels);
        if (tms9918->isUnlocked)
          tempStatus = vrEmuTms9918OutputSprites(VR_EMU_INST y, pixels);
        break;

      case TMS_MODE_MULTICOLOR:
        vrEmuTms9918MulticolorScanLine(VR_EMU_INST y, pixels);
        tempStatus = vrEmuTms9918OutputSprites(VR_EMU_INST y, pixels);
        break;

      case TMS_MODE_TEXT80:
        vrEmuTms9918Text80ScanLine(VR_EMU_INST y, pixels);
        if (tms9918->isUnlocked)
          tempStatus = vrEmuTms9918OutputSprites(VR_EMU_INST y, pixels);
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
  return TMS_REGISTER(tms9918, reg & tms9918->lockedMask); // was 0x07
}

/* Function:  vrEmuTms9918WriteRegValue
 * ----------------------------------------
 * write a reigister value
 */
VR_EMU_TMS9918_DLLEXPORT
void __time_critical_func(vrEmuTms9918WriteRegValue)(VR_EMU_INST_ARG vrEmuTms9918Register reg, uint8_t value)
{
  if ((reg == (0x80 | 0x39)) && ((value & 0xfc) == 0x1c))
  {
    TMS_REGISTER(tms9918, 0x39) = 0x1c; // Allow this one through even when locked
    if (++tms9918->unlockCount == 2)
    {
      tms9918->unlockCount = 0;
      tms9918->isUnlocked = true;
      tms9918->lockedMask = 0x3f;
      TMS_REGISTER(tms9918, 0x1e) = MAX_SPRITES - 1; // Sprites to process
    }
  }
  else
  {
    tms9918->unlockCount = 0;
    
    if ((reg & ~tms9918->lockedMask) != 0x80) return; //ignore higher registers when locked

    int regIndex = reg & tms9918->lockedMask; // was 0x07
    TMS_REGISTER(tms9918, regIndex) = value;
    if ((regIndex == 0x37) || ((regIndex == 0x38) && ((value & 1) == 0)))
    {
      tms9918->gpuAddress = ((TMS_REGISTER(tms9918, 0x36) << 8) | TMS_REGISTER(tms9918, 0x37)) & 0xFFFE;
      if (regIndex == 0x37)
      {
        TMS_REGISTER(tms9918, 0x38) = 0;
        tms9918->restart = 1;
      }
    }
    else if ((regIndex == 0x38) && (value & 1))
    {
      tms9918->restart = 1;
    }
    else if ((regIndex == 0x3F)) // firmware update
    {
      // b7      : 0 = idle:   1 = execute
      // b6      : 0 = verify: 1 = write
      // b5 - b0 : address to read firmware data (256 byte boundaries)
      //           reads one UF2 frame (512 bytes)
      if (TMS_REGISTER(tms9918, 0x38) == 0)
      {
        TMS_STATUS(tms9918, 2) = 0x80; // set gpu processing flag
        tms9918->flash = 1;
      }
      else
      {
        TMS_STATUS(tms9918, 2) = 0x14; // error - busy
      }
    }
    else if (regIndex == 0x1e && value == 0)
    {
      TMS_REGISTER(tms9918, 0x1e) = MAX_SPRITES - 1;
    }
    else if ((regIndex == 0x32) && (value & 0x80))
    { // reset all registers?
      vdpRegisterReset(tms9918);

      // reset palette, etc as well?
      if (value & 0x40)
      {
        tms9918->configDirty = true;
      }
    }
    else if (regIndex == 0x0F)
    {
      uint8_t statReg = (value & 0x0f);
      TMS_STATUS(tms9918, 0x0F) = statReg;  // is this right? or should this be the read-ahead value?
      if (value & 0x40) tms9918->startTime = time_us_32();    // reset
      if (value & 0x20) tms9918->currentTime = time_us_32();  // snap      
      else if (value & 0x10) tms9918->startTime += (tms9918->stopTime - tms9918->startTime);
      else tms9918->currentTime = tms9918->stopTime = time_us_32();

      if (statReg > 3 && statReg < 12)
      {
        uint32_t elapsed = tms9918->currentTime - tms9918->startTime;
        divmod_result_t micro = divmod_u32u32(elapsed, 1000);
        divmod_result_t milli = divmod_u32u32(to_quotient_u32(micro), 1000);

        TMS_STATUS(tms9918, 0x06) = to_remainder_u32(micro) & 0x0ff; 
        TMS_STATUS(tms9918, 0x07) = to_remainder_u32(micro) >> 8;
        TMS_STATUS(tms9918, 0x08) = to_remainder_u32(milli) & 0x0ff;
        TMS_STATUS(tms9918, 0x09) = to_remainder_u32(milli) >> 8;
        TMS_STATUS(tms9918, 0x0a) = to_quotient_u32(milli) & 0x00ff;
        TMS_STATUS(tms9918, 0x0b) = to_quotient_u32(milli) >> 8;
      }
    }
    else if (regIndex == 58)  // SR12 holds the value of the option in VR58 (options)
    {
      TMS_STATUS(tms9918, 12) = tms9918->config[TMS_REGISTER(tms9918, 58)];
    }
    else if (regIndex == 59 && TMS_REGISTER(tms9918, 58) >= 8)  // option number in reg 58, value in 59 (options)
    {
      tms9918->config[TMS_REGISTER(tms9918, 58)] = value;
      TMS_STATUS(tms9918, 12) = value;
      tms9918->configDirty = true;
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
  return tms9918->vram.bytes[addr & VRAM_MASK];
}

/* Function:  vrEmuTms9918DisplayEnabled
  * ----------------------------------------
  * check BLANK flag
  */
VR_EMU_TMS9918_DLLEXPORT
bool __time_critical_func(vrEmuTms9918DisplayEnabled)(VR_EMU_INST_ONLY_ARG)
{
  return (TMS_REGISTER(tms9918, TMS_REG_1) & TMS_R1_DISP_ACTIVE);
}

/* Function:  vrEmuTms9918DisplayMode
  * --------------------
  * current display mode
  */
VR_EMU_TMS9918_DLLEXPORT
vrEmuTms9918Mode __time_critical_func(vrEmuTms9918DisplayMode)(VR_EMU_INST_ONLY_ARG)
{
  return tmsCachedMode;
}

/* Function:  vrEmuTms9918DefaultPalette
  * --------------------
  * a default palette value 0x0rgb
  */
VR_EMU_TMS9918_DLLEXPORT
uint16_t vrEmuTms9918DefaultPalette(int index)
{
  return defaultPalette[index & 0x3f];
}