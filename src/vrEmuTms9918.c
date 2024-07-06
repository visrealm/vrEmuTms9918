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


#ifdef PICO_BUILD
#include "pico/stdlib.h"
#define inline __force_inline
#else
#define __time_critical_func(fn) fn
#endif

#include <stdlib.h>
#include <memory.h>
#include <math.h>
#include <string.h>

#include "vrEmuTms9918.h"

#ifndef WIN32
#undef VR_EMU_TMS9918_DLLEXPORT
#define VR_EMU_TMS9918_DLLEXPORT
#endif


#define VRAM_SIZE           (1 << 14) /* 16KB */
#define VRAM_MASK     (VRAM_SIZE - 1) /* 0x3fff */

#define GRAPHICS_NUM_COLS         32
#define GRAPHICS_NUM_ROWS         24
#define GRAPHICS_CHAR_WIDTH        8

#define TEXT_NUM_COLS             40
#define TEXT_NUM_ROWS             24
#define TEXT_CHAR_WIDTH            6
#define TEXT_PADDING_PX            8
#define TEXT80_NUM_COLS           80

#define PATTERN_BYTES              8
#define GFXI_COLOR_GROUP_SIZE      8

#define MAX_SPRITES               32

#define SPRITE_ATTR_Y              0
#define SPRITE_ATTR_X              1
#define SPRITE_ATTR_NAME           2
#define SPRITE_ATTR_COLOR          3
#define SPRITE_ATTR_BYTES          4
#define LAST_SPRITE_YPOS        0xD0
#define MAX_SCANLINE_SPRITES       4

#define STATUS_INT              0x80
#define STATUS_5S               0x40
#define STATUS_COL              0x20

#define TMS_R0_MODE_TEXT_80     0x04
#define TMS_R0_MODE_GRAPHICS_II 0x02
#define TMS_R0_EXT_VDP_ENABLE   0x01

#define TMS_R1_DISP_ACTIVE      0x40
#define TMS_R1_INT_ENABLE       0x20
#define TMS_R1_MODE_MULTICOLOR  0x08
#define TMS_R1_MODE_TEXT        0x10
#define TMS_R1_SPRITE_16        0x02
#define TMS_R1_SPRITE_MAG2      0x01

 /* PRIVATE DATA STRUCTURE
  * ---------------------- */
struct vrEmuTMS9918_s
{
  /* the eight write-only registers */
  uint8_t registers[TMS_NUM_REGISTERS];

  /* status register (read-only) */
  uint8_t status;

  /* current address for cpu access (auto-increments) */
  uint16_t currentAddress;

  /* address or register write stage (0 or 1) */
  uint8_t regWriteStage;

  /* holds first stage of write to address/register port */
  uint8_t regWriteStage0Value;

  /* buffered value */
  uint8_t readAheadBuffer;

  bool interruptsEnabled;

  /* current display mode */
  vrEmuTms9918Mode mode;

  /* cached values */
  uint8_t spriteSize;
  bool spriteMag;
  uint16_t nameTableAddr;
  uint16_t colorTableAddr;
  uint16_t patternTableAddr;
  uint16_t spriteAttrTableAddr;
  uint16_t spritePatternTableAddr;
  vrEmuTms9918Color mainBgColor;
  vrEmuTms9918Color mainFgColor;
  bool displayEnabled;

  /* video ram */
  uint8_t vram[VRAM_SIZE];

  uint32_t rowSpriteBits[TMS9918_PIXELS_X / 32]; /* collision mask */
};


#if VR_EMU_TMS9918_SINGLE_INSTANCE

static __aligned(4) VrEmuTms9918 tms9918Inst;
static VrEmuTms9918* tms9918 = &tms9918Inst;

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
static inline void tmsMemset(uint8_t* ptr, uint8_t val8, int count)
{
  uint32_t* val32 = (uint32_t*)ptr;

  *ptr++ = val8;
  *ptr++ = val8;
  *ptr++ = val8;
  *ptr++ = val8;

  count >>= 2;

  uint32_t* ptr32 = (uint32_t*)ptr;

  while (--count)
  {
    *ptr32++ = *val32;
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
  free(tms9918);
  tms9918 = NULL;
}

/* Function:  vrEmuTms9918WriteAddr
 * ----------------------------------------
 * write an address (mode = 1) to the tms9918
 *
 * data: the data (DB0 -> DB7) to send
 */
VR_EMU_TMS9918_DLLEXPORT void __time_critical_func(vrEmuTms9918WriteAddr)(VR_EMU_INST_ARG uint8_t data)
{
  if (tms9918->regWriteStage == 0)
  {
    /* first stage byte - either an address LSB or a register value */

    tms9918->regWriteStage0Value = data;
    tms9918->regWriteStage = 1;
  }
  else
  {
    /* second byte - either a register number or an address MSB */

    if (data & 0x80) /* register */
    {
      if ((data & 0x78) == 0)
      {
        vrEmuTms9918WriteRegValue(data, tms9918->regWriteStage0Value);
      }
    }
    else /* address */
    {
      tms9918->currentAddress = tms9918->regWriteStage0Value | ((data & 0x3f) << 8);
      if ((data & 0x40) == 0)
      {
        tms9918->readAheadBuffer = tms9918->vram[(tms9918->currentAddress++) & VRAM_MASK];
      }
    }
    tms9918->regWriteStage = 0;
  }
}

/* Function:  vrEmuTms9918ReadStatus
 * ----------------------------------------
 * read from the status register
 */
VR_EMU_TMS9918_DLLEXPORT uint8_t __time_critical_func(vrEmuTms9918ReadStatus)(VR_EMU_INST_ONLY_ARG)
{
  const uint8_t tmpStatus = tms9918->status;
  tms9918->status &= ~(STATUS_INT | STATUS_COL);
  tms9918->regWriteStage = 0;
  return tmpStatus;
}

/* Function:  vrEmuTms9918PeekStatus
 * ----------------------------------------
 * read from the status register without resetting it
 */
VR_EMU_TMS9918_DLLEXPORT uint8_t __time_critical_func(vrEmuTms9918PeekStatus)(VR_EMU_INST_ONLY_ARG)
{
  return tms9918->status;
}

/* Function:  vrEmuTms9918WriteData
 * ----------------------------------------
 * write data (mode = 0) to the tms9918
 *
 * data: the data (DB0 -> DB7) to send
 */
VR_EMU_TMS9918_DLLEXPORT void __time_critical_func(vrEmuTms9918WriteData)(VR_EMU_INST_ARG uint8_t data)
{
  tms9918->regWriteStage = 0;
  tms9918->readAheadBuffer = data;
  tms9918->vram[(tms9918->currentAddress++) & VRAM_MASK] = data;
}


/* Function:  vrEmuTms9918ReadData
 * ----------------------------------------
 * read data (mode = 0) from the tms9918
 */
VR_EMU_TMS9918_DLLEXPORT uint8_t __time_critical_func(vrEmuTms9918ReadData)(VR_EMU_INST_ONLY_ARG)
{
  tms9918->regWriteStage = 0;
  uint8_t currentValue = tms9918->readAheadBuffer;
  tms9918->readAheadBuffer = tms9918->vram[(tms9918->currentAddress++) & VRAM_MASK];
  return currentValue;
}

/* Function:  vrEmuTms9918ReadDataNoInc
 * ----------------------------------------
 * read data (mode = 0) from the tms9918
 */
VR_EMU_TMS9918_DLLEXPORT uint8_t __time_critical_func(vrEmuTms9918ReadDataNoInc)(VR_EMU_INST_ONLY_ARG)
{
  return tms9918->readAheadBuffer;
}

/* Function:  vrEmuTms9918InterruptStatus
 * --------------------
 * return true if both INT status and INT control set
 */
VR_EMU_TMS9918_DLLEXPORT bool __time_critical_func(vrEmuTms9918InterruptStatus)(VR_EMU_INST_ONLY_ARG)
{
  return tms9918->interruptsEnabled && (tms9918->status & STATUS_INT);
}

/* Function:  vrEmuTms9918InterruptSet
 * --------------------
 * return true if both INT status and INT control set
 */
VR_EMU_TMS9918_DLLEXPORT void __time_critical_func(vrEmuTms9918InterruptSet)(VR_EMU_INST_ONLY_ARG)
{
  tms9918->status |= STATUS_INT;
}

/* Function:  vrEmuTms9918SetStatus
 * --------------------
 * set status flag
 */
VR_EMU_TMS9918_DLLEXPORT
void vrEmuTms9918SetStatus(VR_EMU_INST_ARG uint8_t status)
{
  tms9918->status = status;
}

static uint16_t __aligned(4) doubled[256] = {
    0x0000, 0x0003, 0x000c, 0x000f, 0x0030, 0x0033, 0x003c, 0x003f,
    0x00c0, 0x00c3, 0x00cc, 0x00cf, 0x00f0, 0x00f3, 0x00fc, 0x00ff,
    0x0300, 0x0303, 0x030c, 0x030f, 0x0330, 0x0333, 0x033c, 0x033f,
    0x03c0, 0x03c3, 0x03cc, 0x03cf, 0x03f0, 0x03f3, 0x03fc, 0x03ff,
    0x0c00, 0x0c03, 0x0c0c, 0x0c0f, 0x0c30, 0x0c33, 0x0c3c, 0x0c3f,
    0x0cc0, 0x0cc3, 0x0ccc, 0x0ccf, 0x0cf0, 0x0cf3, 0x0cfc, 0x0cff,
    0x0f00, 0x0f03, 0x0f0c, 0x0f0f, 0x0f30, 0x0f33, 0x0f3c, 0x0f3f,
    0x0fc0, 0x0fc3, 0x0fcc, 0x0fcf, 0x0ff0, 0x0ff3, 0x0ffc, 0x0fff,
    0x3000, 0x3003, 0x300c, 0x300f, 0x3030, 0x3033, 0x303c, 0x303f,
    0x30c0, 0x30c3, 0x30cc, 0x30cf, 0x30f0, 0x30f3, 0x30fc, 0x30ff,
    0x3300, 0x3303, 0x330c, 0x330f, 0x3330, 0x3333, 0x333c, 0x333f,
    0x33c0, 0x33c3, 0x33cc, 0x33cf, 0x33f0, 0x33f3, 0x33fc, 0x33ff,
    0x3c00, 0x3c03, 0x3c0c, 0x3c0f, 0x3c30, 0x3c33, 0x3c3c, 0x3c3f,
    0x3cc0, 0x3cc3, 0x3ccc, 0x3ccf, 0x3cf0, 0x3cf3, 0x3cfc, 0x3cff,
    0x3f00, 0x3f03, 0x3f0c, 0x3f0f, 0x3f30, 0x3f33, 0x3f3c, 0x3f3f,
    0x3fc0, 0x3fc3, 0x3fcc, 0x3fcf, 0x3ff0, 0x3ff3, 0x3ffc, 0x3fff,
    0xc000, 0xc003, 0xc00c, 0xc00f, 0xc030, 0xc033, 0xc03c, 0xc03f,
    0xc0c0, 0xc0c3, 0xc0cc, 0xc0cf, 0xc0f0, 0xc0f3, 0xc0fc, 0xc0ff,
    0xc300, 0xc303, 0xc30c, 0xc30f, 0xc330, 0xc333, 0xc33c, 0xc33f,
    0xc3c0, 0xc3c3, 0xc3cc, 0xc3cf, 0xc3f0, 0xc3f3, 0xc3fc, 0xc3ff,
    0xcc00, 0xcc03, 0xcc0c, 0xcc0f, 0xcc30, 0xcc33, 0xcc3c, 0xcc3f,
    0xccc0, 0xccc3, 0xcccc, 0xcccf, 0xccf0, 0xccf3, 0xccfc, 0xccff,
    0xcf00, 0xcf03, 0xcf0c, 0xcf0f, 0xcf30, 0xcf33, 0xcf3c, 0xcf3f,
    0xcfc0, 0xcfc3, 0xcfcc, 0xcfcf, 0xcff0, 0xcff3, 0xcffc, 0xcfff,
    0xf000, 0xf003, 0xf00c, 0xf00f, 0xf030, 0xf033, 0xf03c, 0xf03f,
    0xf0c0, 0xf0c3, 0xf0cc, 0xf0cf, 0xf0f0, 0xf0f3, 0xf0fc, 0xf0ff,
    0xf300, 0xf303, 0xf30c, 0xf30f, 0xf330, 0xf333, 0xf33c, 0xf33f,
    0xf3c0, 0xf3c3, 0xf3cc, 0xf3cf, 0xf3f0, 0xf3f3, 0xf3fc, 0xf3ff,
    0xfc00, 0xfc03, 0xfc0c, 0xfc0f, 0xfc30, 0xfc33, 0xfc3c, 0xfc3f,
    0xfcc0, 0xfcc3, 0xfccc, 0xfccf, 0xfcf0, 0xfcf3, 0xfcfc, 0xfcff,
    0xff00, 0xff03, 0xff0c, 0xff0f, 0xff30, 0xff33, 0xff3c, 0xff3f,
    0xffc0, 0xffc3, 0xffcc, 0xffcf, 0xfff0, 0xfff3, 0xfffc, 0xffff
};

/* Function:  tmsTestCollisionMask
 * ----------------------------------------
 * Test and update the sprite collision mask.
 */
static inline uint32_t tmsTestCollisionMask(uint8_t xPos, uint32_t spritePixels, uint8_t spriteWidth)
{
  int rowSpriteBitsWord = xPos >> 5;
  int rowSpriteBitsWordBit = xPos & 0x1f;
  uint32_t validPixels = 0;

  validPixels = ~tms9918->rowSpriteBits[rowSpriteBitsWord] & (spritePixels >> rowSpriteBitsWordBit);
  tms9918->rowSpriteBits[rowSpriteBitsWord] |= validPixels;
  validPixels <<= rowSpriteBitsWordBit;

  if ((rowSpriteBitsWordBit + spriteWidth) > 32)
  {
    uint32_t right = ~tms9918->rowSpriteBits[rowSpriteBitsWord + 1] & (spritePixels << (32 - rowSpriteBitsWordBit));
    tms9918->rowSpriteBits[rowSpriteBitsWord + 1] |= right;
    validPixels |= (right >> (32 - rowSpriteBitsWordBit));
  }

  return validPixels;
}

/* Function:  vrEmuTms9918OutputSprites
 * ----------------------------------------
 * Output Sprites to a scanline
 */
static uint8_t __time_critical_func(vrEmuTms9918OutputSprites)(VR_EMU_INST_ARG uint8_t y, uint8_t pixels[TMS9918_PIXELS_X], uint8_t status)
{
  const bool spriteMag = tms9918->spriteMag;
  const uint8_t spriteSize = tms9918->spriteSize;
  const bool sprite16 = spriteSize == 16;
  const uint8_t spriteSizePx = spriteSize * (spriteMag + 1);
  const uint16_t spriteAttrTableAddr = tms9918->spriteAttrTableAddr;
  const uint16_t spritePatternAddr = tms9918->spritePatternTableAddr;

  uint8_t spritesShown = 0;
  status &= (STATUS_INT | STATUS_COL);

  uint8_t* spriteAttr = tms9918->vram + spriteAttrTableAddr;
  for (uint8_t spriteIdx = 0; spriteIdx < MAX_SPRITES; ++spriteIdx)
  {
    int16_t yPos = spriteAttr[SPRITE_ATTR_Y];

    /* stop processing when yPos == LAST_SPRITE_YPOS */
    if (yPos == LAST_SPRITE_YPOS)
    {
      if ((status & STATUS_5S) == 0)
      {
        status &= 0xe0;
        status |= spriteIdx;
      }
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
      if ((status & (STATUS_5S | STATUS_INT)) == 0)
      {
        status &= 0xe0;
        status |= STATUS_5S | spriteIdx;
      }
      break;
    }

    /* sprite is visible on this line */
    const uint8_t spriteColor = spriteAttr[SPRITE_ATTR_COLOR] & 0x0f;
    const uint8_t pattIdx = spriteAttr[SPRITE_ATTR_NAME];
    const uint16_t pattOffset = spritePatternAddr + pattIdx * PATTERN_BYTES + (uint16_t)pattRow;

    const int16_t earlyClockOffset = (spriteAttr[SPRITE_ATTR_COLOR] & 0x80) ? -32 : 0;
    int16_t xPos = (int16_t)(spriteAttr[SPRITE_ATTR_X]) + earlyClockOffset;

    uint8_t pattByte = tms9918->vram[pattOffset & VRAM_MASK];

    /* create a 32-bit mask of this sprite's pixels
     * left-aligned, so the first pixel in the sprite is the
     * MSB of pattMask
     */
    uint32_t pattMask = spriteMag ? doubled[pattByte] : pattByte;

    if (sprite16)
    {
      /* grab the next byte too */
      pattByte = tms9918->vram[(pattOffset + PATTERN_BYTES * 2) & VRAM_MASK];
      if (spriteMag)
        pattMask = (pattMask << 16) | doubled[pattByte];
      else
        pattMask = (pattMask << 8) | pattByte;
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
      status |= STATUS_COL;
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
  return status;
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

  uint8_t* rowNamesTable = tms9918->vram /*+ tmsNameTableAddr(tms9918)*/ + tileY * TEXT80_NUM_COLS;
  const uint8_t* patternTable = tms9918->vram + tms9918->patternTableAddr + pattRow;

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

  uint32_t* pixPtr = (uint32_t*)pixels;

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
static void __time_critical_func(vrEmuTms9918GraphicsIIScanLine)(VR_EMU_INST_ARG uint8_t y, uint8_t pixels[TMS9918_PIXELS_X])
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
VR_EMU_TMS9918_DLLEXPORT uint8_t __time_critical_func(vrEmuTms9918ScanLine)(VR_EMU_INST_ARG uint8_t y, uint8_t pixels[TMS9918_PIXELS_X], uint8_t status)
{
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
        status = vrEmuTms9918OutputSprites(VR_EMU_INST y, pixels, status);
        break;

      case TMS_MODE_GRAPHICS_II:
        vrEmuTms9918GraphicsIIScanLine(VR_EMU_INST y, pixels);
        status = vrEmuTms9918OutputSprites(VR_EMU_INST y, pixels, status);
        break;

      case TMS_MODE_TEXT:
        vrEmuTms9918TextScanLine(VR_EMU_INST y, pixels);
        break;

      case TMS_MODE_MULTICOLOR:
        vrEmuTms9918MulticolorScanLine(VR_EMU_INST y, pixels);
        status = vrEmuTms9918OutputSprites(VR_EMU_INST y, pixels, status);
        break;

      case TMS_R0_MODE_TEXT_80:
        vrEmuTms9918Text80ScanLine(VR_EMU_INST y, pixels);
        break;
    }
  }

  return status;
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
