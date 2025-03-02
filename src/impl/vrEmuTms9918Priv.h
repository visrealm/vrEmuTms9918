#pragma once

#ifdef PICO_BUILD
#include "pico/stdlib.h"
#define inline __force_inline
#else
#define __time_critical_func(fn) fn
#endif

#include "../vrEmuTms9918.h"


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

  /* maximum number of sprites on a scanline */
  int maxScanlineSprites;

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
extern VrEmuTms9918* tms9918;
#endif

/* Function:  vrEmuTms9918WriteAddr
 * ----------------------------------------
 * write an address (mode = 1) to the tms9918
 *
 * data: the data (DB0 -> DB7) to send
 */
inline void vrEmuTms9918WriteAddrImpl(VR_EMU_INST_ARG uint8_t data)
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
inline uint8_t vrEmuTms9918ReadStatusImpl(VR_EMU_INST_ONLY_ARG)
{
  const uint8_t tmpStatus = tms9918->status;
  tms9918->status = 0x1f;
  tms9918->regWriteStage = 0;
  return tmpStatus;
}

/* Function:  vrEmuTms9918PeekStatus
 * ----------------------------------------
 * read from the status register without resetting it
 */
inline uint8_t vrEmuTms9918PeekStatusImpl(VR_EMU_INST_ONLY_ARG)
{
  return tms9918->status;
}

/* Function:  vrEmuTms9918WriteData
 * ----------------------------------------
 * write data (mode = 0) to the tms9918
 *
 * data: the data (DB0 -> DB7) to send
 */
inline void vrEmuTms9918WriteDataImpl(VR_EMU_INST_ARG uint8_t data)
{
  tms9918->regWriteStage = 0;
  tms9918->readAheadBuffer = data;
  tms9918->vram[(tms9918->currentAddress++) & VRAM_MASK] = data;
}


/* Function:  vrEmuTms9918ReadData
 * ----------------------------------------
 * read data (mode = 0) from the tms9918
 */
inline uint8_t vrEmuTms9918ReadDataImpl(VR_EMU_INST_ONLY_ARG)
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
inline uint8_t vrEmuTms9918ReadDataNoIncImpl(VR_EMU_INST_ONLY_ARG)
{
  return tms9918->readAheadBuffer;
}

/* Function:  vrEmuTms9918InterruptStatus
 * --------------------
 * return true if both INT status and INT control set
 */
inline bool vrEmuTms9918InterruptStatusImpl(VR_EMU_INST_ONLY_ARG)
{
  return tms9918->interruptsEnabled && (tms9918->status & STATUS_INT);
}

/* Function:  vrEmuTms9918InterruptSet
 * --------------------
 * return true if both INT status and INT control set
 */
inline void vrEmuTms9918InterruptSetImpl(VR_EMU_INST_ONLY_ARG)
{
  tms9918->status |= STATUS_INT;
}

/* Function:  vrEmuTms9918SetStatus
 * --------------------
 * set status flag
 */
inline
void vrEmuTms9918SetStatusImpl(VR_EMU_INST_ARG uint8_t status)
{
  tms9918->status = status;
}