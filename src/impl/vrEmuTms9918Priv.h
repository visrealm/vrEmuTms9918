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
  /* video ram */
  uint8_t vram[VRAM_SIZE];      // 0x0000-0x3FFF (16KB)
  uint8_t  gram1[0x1000];         // 0x4000-0x4fff (4KB) 2x repeated 2KB
  uint16_t pram[0x0800];          // 0x5000-0x5fff (4KB) 32x repeated 128B

  /* 64 write-only registers */
  uint8_t registers[0x40];      // 0x6000-0x6040

  uint8_t gram2[0x1000 - 0x40]; // 0x6040-0x6FFF (~4KB)
  uint8_t scanline;             // 0x7000
  uint8_t blanking;             // 0x7001
  uint8_t gram3[0x4000 - 0x02]; // 0x7002-0xAFFF (~16KB)

  /* status registers (read-only) */
  uint8_t status [0x10];        // 0xB000

  uint8_t gram4[0x5000 - 0x10]; // 0xB010-0xFFFF (~20KB)
  uint8_t wrksp[36];           // 0x10000 overflow for hidden workspace

  /* current address for cpu access (auto-increments) */
  uint16_t currentAddress;

  uint16_t gpuAddress;

  /* address or register write stage (0 or 1) */
  uint8_t regWriteStage;

  /* holds first stage of write to address/register port */
  uint8_t regWriteStage0Value;

  /* buffered value */
  uint8_t readAheadBuffer;

  uint8_t lockedMask;  // 0x07 when locked, 0x3F when unlocked
  uint8_t unlockCount; // number of unlock steps taken
  bool isUnlocked;    // boolean version of lockedMask
  
  volatile uint8_t restart;

  /* palette writes are done in two stages too */
  uint8_t palWriteStage;
  uint8_t palWriteStage0Value;

  uint64_t startTime;
  uint64_t stopTime;
  uint64_t currentTime;

  bool scanlineHasSprites;
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
      if ((data & 0x40) == 0) // Was 0x78, but we allow 64 registers = 0x40
      {
        vrEmuTms9918WriteRegValue(VR_EMU_INST data, tms9918->regWriteStage0Value);
      }
    }
    else /* address */
    {
      tms9918->currentAddress = tms9918->regWriteStage0Value | ((data & 0x3f) << 8);
      if ((data & 0x40) == 0)
      {
        tms9918->readAheadBuffer = tms9918->vram[(tms9918->currentAddress) & VRAM_MASK];
        tms9918->currentAddress += (int8_t)tms9918->registers[0x30]; // increment register
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
  tms9918->regWriteStage = 0;
  
  tms9918->palWriteStage = 0;
  tms9918->registers[0x2f] &= 0x7f; // reset data port palette mode

  if ((tms9918->registers [0x0F] & 0x0F) == 0) {
    const uint8_t tmpStatus = tms9918->status [0];
    tms9918->status [0] = 0x1f;
    return tmpStatus;
  } else
    return tms9918->status [tms9918->registers [0x0F] & 0x0F];
}

/* Function:  vrEmuTms9918PeekStatus
 * ----------------------------------------
 * read from the status register without resetting it
 */
inline uint8_t vrEmuTms9918PeekStatusImpl(VR_EMU_INST_ONLY_ARG)
{
  return tms9918->status [0];
}

/* Function:  vrEmuTms9918WriteData
 * ----------------------------------------
 * write data (mode = 0) to the tms9918
 *
 * data: the data (DB0 -> DB7) to send
 */
inline void vrEmuTms9918WriteDataImpl(VR_EMU_INST_ARG uint8_t data)
{
  if (tms9918->registers[0x2f] & 0x80) // data port is in palette mode
  {
    if (tms9918->palWriteStage == 0)
    {
      tms9918->palWriteStage0Value = data & 0x0f;
      ++tms9918->palWriteStage;      
    }
    else
    {
      tms9918->palWriteStage = 0;

      // this looks backwards because ARM is little-endian, TMS9900 is big-endian. 
      tms9918->pram[tms9918->registers[0x2f] & 0x3f] = (tms9918->palWriteStage0Value) | (data << 8); 

      // reset data port palette mode
      if (tms9918->registers[0x2f] & 0x40)
      {
        ++tms9918->registers[0x2f];
      }
      else
      {
        tms9918->registers[0x2f] &= 0x7f;
      }
    }
  }
  else
  {
    tms9918->regWriteStage = 0;
    tms9918->readAheadBuffer = data;
    tms9918->vram[(tms9918->currentAddress) & VRAM_MASK] = data;
    tms9918->currentAddress += (int8_t)tms9918->registers[0x30]; // increment register
  }
}


/* Function:  vrEmuTms9918ReadData
 * ----------------------------------------
 * read data (mode = 0) from the tms9918
 */
inline uint8_t vrEmuTms9918ReadDataImpl(VR_EMU_INST_ONLY_ARG)
{
  tms9918->regWriteStage = 0;
  uint8_t currentValue = tms9918->readAheadBuffer;
  tms9918->readAheadBuffer = tms9918->vram[(tms9918->currentAddress) & VRAM_MASK];
  tms9918->currentAddress += (int8_t)tms9918->registers[0x30]; // increment register
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
  return (tms9918->registers[TMS_REG_1] & TMS_R1_INT_ENABLE) && (tms9918->status [0] & STATUS_INT);
}

/* Function:  vrEmuTms9918InterruptSet
 * --------------------
 * return true if both INT status and INT control set
 */
inline void vrEmuTms9918InterruptSetImpl(VR_EMU_INST_ONLY_ARG)
{
  tms9918->status [0] |= STATUS_INT;
}

/* Function:  vrEmuTms9918SetStatus
 * --------------------
 * set status flag
 */
inline
void vrEmuTms9918SetStatusImpl(VR_EMU_INST_ARG uint8_t status)
{
  tms9918->status [0] = status;
}