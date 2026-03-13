#pragma once

#ifdef PICO_BUILD
#include "pico/stdlib.h"
#define inline __force_inline
#else
#define __time_critical_func(fn) fn
#endif

#include "../vrEmuTms9918.h"


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

#define VR_EMU_TMS9918_MODE_TMS9918 0
#define VR_EMU_TMS9918_MODE_F18A    1
#define VR_EMU_TMS9918_MODE_V9938   2

#ifndef VR_EMU_TMS9918_MODE
#define VR_EMU_TMS9918_MODE VR_EMU_TMS9918_MODE_TMS9918
#endif

/* VRAM address auto-increment: F18A uses a configurable register (R#48);
 * V9938 and TMS9918 always increment by 1. */
#if VR_EMU_TMS9918_MODE == VR_EMU_TMS9918_MODE_F18A
  #define TMS_ADDR_INC(t) ((int8_t)TMS_REGISTER(t, 0x30))
#else
  #define TMS_ADDR_INC(t) (1)
#endif

#define BASE_VRAM_SIZE        (1 << 14) /* 16kB */

#if VR_EMU_TMS9918_MODE == VR_EMU_TMS9918_MODE_F18A
  #define VRAM_SIZE            (1 << 16) /* 64kB */
  #define TMS_REGISTERS        64
  #define TMS_STATUS_REGISTERS 16
  #define MAPPED_REGISTERS     1
  #define MAPPED_STATUS        1
#elif VR_EMU_TMS9918_MODE == VR_EMU_TMS9918_MODE_V9938
  #define VRAM_SIZE            (1 << 17) /* 128kB */
  #define TMS_REGISTERS        64
  #define TMS_STATUS_REGISTERS 10
  #define MAPPED_REGISTERS     0
  #define MAPPED_STATUS        0
#else
  #define VRAM_SIZE            BASE_VRAM_SIZE
  #define TMS_REGISTERS        16
  #define TMS_STATUS_REGISTERS 1
  #define MAPPED_REGISTERS     0
  #define MAPPED_STATUS        0
#endif

#if VR_EMU_TMS9918_MODE == VR_EMU_TMS9918_MODE_V9938
  #define VRAM_MASK   (VRAM_SIZE - 1)       /* 0x1ffff - 17-bit */
#else
  #define VRAM_MASK   (BASE_VRAM_SIZE - 1)  /* 0x3fff  - 14-bit */
#endif


typedef struct
{
  uint8_t  base[BASE_VRAM_SIZE];                 // 0x0000-0x3FFF (16KB)
#if VR_EMU_TMS9918_MODE == VR_EMU_TMS9918_MODE_V9938
  uint8_t  ext[VRAM_SIZE - BASE_VRAM_SIZE];      // 0x4000-0x1FFFF (extended 112KB)
#elif VR_EMU_TMS9918_MODE == VR_EMU_TMS9918_MODE_F18A
  /* video ram */
  uint8_t  gram1[0x1000];                       // 0x4000-0x4fff (4KB) 2x repeated 2KB
  uint16_t pram[0x0800];                        // 0x5000-0x5fff (4KB) 32x repeated 128B

  /* 64 write-only registers */
  uint8_t  registers[TMS_REGISTERS];             // 0x6000-0x6040

  uint8_t  gram2[0x1000 - TMS_REGISTERS];        // 0x6040-0x6FFF (~4KB)
  uint8_t  scanline;                             // 0x7000
  uint8_t  blanking;                             // 0x7001
  uint8_t  gram3[0x4000 - 2];                    // 0x7002-0xAFFF (~16KB)

  /* status registers (read-only) */
  uint8_t  status [TMS_STATUS_REGISTERS];        // 0xB000

  uint8_t  gram4[0x5000 - TMS_STATUS_REGISTERS]; // 0xB010-0xFFFF (~20KB)
  uint8_t  wrksp[36];                            // 0x10000 overflow for hidden workspace
#endif
} vrEmuTMS9918MemMap;

#if MAPPED_REGISTERS
  #define TMS_REGISTER(T, R)      (T->vram.map.registers[R])
#else
  #define TMS_REGISTER(T, R)      (T->registers[R])
#endif

#if MAPPED_STATUS
  #define TMS_STATUS(T, R)      (T->vram.map.status[R])
#else
  #define TMS_STATUS(T, R)      (T->status[R])
#endif



 /* PRIVATE DATA STRUCTURE
  * ---------------------- */
struct vrEmuTMS9918_s
{
  union 
  {
    uint8_t bytes[VRAM_SIZE];
    vrEmuTMS9918MemMap map;
  } vram;

#if !MAPPED_REGISTERS
  uint8_t registers[TMS_REGISTERS];
#endif

#if !MAPPED_STATUS
  uint8_t status[TMS_STATUS_REGISTERS];
#endif

  /* current address for cpu access (auto-increments)
   * V9938: 17-bit address (A0-A16); stored as 32-bit for alignment */
  uint32_t currentAddress;

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
  volatile uint8_t flash;

  /* palette writes are done in two stages too */
  uint8_t palWriteStage;
  uint8_t palWriteStage0Value;
  uint8_t palDirty;

  uint32_t startTime;
  uint32_t stopTime;
  uint32_t currentTime;

  uint8_t config[256];
  bool configDirty;

  bool scanlineHasSprites;

#if VR_EMU_TMS9918_MODE == VR_EMU_TMS9918_MODE_V9938
  /* V9938 palette stored in F18A format: 0xARGB (4-bit per channel, alpha=0xF)
   * Matches defaultPalette[] and config layout; conversion from V9938 3-bit
   * wire format happens in vrEmuTms9918WritePaletteImpl(). */
  uint16_t v9938Palette[16];

  /* V9938 command processor state */
  volatile uint8_t cmdActive;  /* non-zero while a command is executing */

  /* V9938: blanking and scanline not in VRAM map, tracked separately */
  uint8_t blanking;
  uint8_t scanline;
#endif
};

/* Macros to access blanking/scanline state regardless of mode */
#if VR_EMU_TMS9918_MODE == VR_EMU_TMS9918_MODE_F18A
  #define TMS_BLANKING(t)       (t)->vram.map.blanking
  #define TMS_SCANLINE_STATE(t) (t)->vram.map.scanline
#elif VR_EMU_TMS9918_MODE == VR_EMU_TMS9918_MODE_V9938
  #define TMS_BLANKING(t)       (t)->blanking
  #define TMS_SCANLINE_STATE(t) (t)->scanline
#else
  /* TMS9918 / base mode: no blanking/scanline tracking needed */
  #define TMS_BLANKING(t)       (0)
  #define TMS_SCANLINE_STATE(t) (0)
#endif

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
#if VR_EMU_TMS9918_MODE == VR_EMU_TMS9918_MODE_V9938
      /* V9938: 17-bit address - A7-A0 from first byte, A13-A8 from second byte,
       * A16-A14 from R#14 bits 2-0 */
      tms9918->currentAddress = tms9918->regWriteStage0Value
        | ((data & 0x3f) << 8)
        | ((uint32_t)(TMS_REGISTER(tms9918, 14) & 0x07) << 14);
#else
      tms9918->currentAddress = tms9918->regWriteStage0Value | ((data & 0x3f) << 8);
#endif
      if ((data & 0x40) == 0)
      {
        tms9918->readAheadBuffer = tms9918->vram.bytes[(tms9918->currentAddress) & VRAM_MASK];
        tms9918->currentAddress += TMS_ADDR_INC(tms9918); // increment VRAM address
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

#if VR_EMU_TMS9918_MODE == VR_EMU_TMS9918_MODE_V9938
  /* V9938: R#15 selects which status register to read; resets to 0 after read */
  uint8_t statIdx = TMS_REGISTER(tms9918, 0x0F) & 0x0f;
  uint8_t result = TMS_STATUS(tms9918, statIdx);
  if (statIdx == 0)
  {
    /* S#0: clear INT (bit 7), 5S (bit 6), C (bit 5) on read */
    TMS_STATUS(tms9918, 0) = result & 0x1f;
  }
  TMS_REGISTER(tms9918, 0x0F) = 0; /* R#15 resets to 0 after status read */
  return result;
#else
  tms9918->palWriteStage = 0;
  TMS_REGISTER(tms9918, 0x2f) &= 0x7f; // reset data port palette mode

  if ((TMS_REGISTER(tms9918, 0x0F) & 0x0F) == 0)
  {
    const uint8_t tmpStatus = TMS_STATUS(tms9918, 0);
    TMS_STATUS(tms9918, 0) = 0x1f;
    return tmpStatus;
  }
  else
  {
    return TMS_STATUS(tms9918, TMS_REGISTER(tms9918, 0x0F) & 0x0F);
  }
#endif
}

/* Function:  vrEmuTms9918PeekStatus
 * ----------------------------------------
 * read from the status register without resetting it
 */
inline uint8_t vrEmuTms9918PeekStatusImpl(VR_EMU_INST_ONLY_ARG)
{
  return TMS_STATUS(tms9918, 0);
}

/* Function:  vrEmuTms9918WriteData
 * ----------------------------------------
 * write data (mode = 0) to the tms9918
 *
 * data: the data (DB0 -> DB7) to send
 */
inline void vrEmuTms9918WriteDataImpl(VR_EMU_INST_ARG uint8_t data)
{
#if VR_EMU_TMS9918_MODE == VR_EMU_TMS9918_MODE_F18A
  if (TMS_REGISTER(tms9918, 0x2f) & 0x80) // data port is in palette mode
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
      tms9918->vram.map.pram[TMS_REGISTER(tms9918, 0x2f) & 0x3f] = (tms9918->palWriteStage0Value) | (data << 8);
      tms9918->palDirty = 1;

      // reset data port palette mode
      if (TMS_REGISTER(tms9918, 0x2f) & 0x40)
      {
        ++TMS_REGISTER(tms9918, 0x2f);
      }
      else
      {
        TMS_REGISTER(tms9918, 0x2f) &= 0x7f;
      }
    }
  }
  else
#endif /* VR_EMU_TMS9918_MODE_F18A */
  {
    tms9918->regWriteStage = 0;
    tms9918->readAheadBuffer = data;
    tms9918->vram.bytes[(tms9918->currentAddress) & VRAM_MASK] = data;
    tms9918->currentAddress += TMS_ADDR_INC(tms9918); // increment VRAM address
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
  tms9918->readAheadBuffer = tms9918->vram.bytes[(tms9918->currentAddress) & VRAM_MASK];
  tms9918->currentAddress += TMS_ADDR_INC(tms9918); // increment VRAM address
  return currentValue;
}

/* Function:  vrEmuTms9918ReadAheadData
 * ----------------------------------------
 * read data (mode = 0) from the tms9918
 */
inline uint8_t vrEmuTms9918ReadAheadDataImpl(VR_EMU_INST_ONLY_ARG)
{
  tms9918->regWriteStage = 0;
  tms9918->readAheadBuffer = tms9918->vram.bytes[(tms9918->currentAddress) & VRAM_MASK];
  tms9918->currentAddress += TMS_ADDR_INC(tms9918); // increment VRAM address
  return tms9918->readAheadBuffer;
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
  return (TMS_REGISTER(tms9918, TMS_REG_1) & TMS_R1_INT_ENABLE) && (TMS_STATUS(tms9918, 0) & STATUS_INT);
}

/* Function:  vrEmuTms9918InterruptSet
 * --------------------
 * return true if both INT status and INT control set
 */
inline void vrEmuTms9918InterruptSetImpl(VR_EMU_INST_ONLY_ARG)
{
  TMS_STATUS(tms9918, 0) |= STATUS_INT;
}

/* Function:  vrEmuTms9918SetStatus
 * --------------------
 * set status flag
 */
inline void vrEmuTms9918SetStatusImpl(VR_EMU_INST_ARG uint8_t status)
{
  TMS_STATUS(tms9918, 0) = status;
}

#if VR_EMU_TMS9918_MODE == VR_EMU_TMS9918_MODE_V9938
/* Function:  vrEmuTms9918WritePaletteImpl
 * ----------------------------------------
 * V9938 palette port write (MODE1=1, MODE0=0 during CSW)
 *
 * V9938 wire format (2-byte write sequence):
 *   Byte 1: 0RRR0BBB  (3-bit red in bits 6-4, 3-bit blue in bits 2-0)
 *   Byte 2: 00000GGG  (3-bit green in bits 2-0)
 * Stored internally as F18A format: 0xARGB (4-bit per channel, alpha=0xF),
 * matching the config and defaultPalette layout throughout the system.
 * R#16 holds palette index (0-15), auto-increments after each complete write.
 */
inline void vrEmuTms9918WritePaletteImpl(VR_EMU_INST_ARG uint8_t data)
{
  if (tms9918->palWriteStage == 0)
  {
    tms9918->palWriteStage0Value = data;  /* 0RRR0BBB */
    tms9918->palWriteStage = 1;
  }
  else
  {
    tms9918->palWriteStage = 0;
    uint8_t palIdx = TMS_REGISTER(tms9918, 0x10) & 0x0f;  /* R#16 */
    /* Convert V9938 3-bit channels to F18A 4-bit format (0xARGB) by doubling (<<1) */
    uint8_t r4 = ((tms9918->palWriteStage0Value >> 4) & 0x07) << 1;
    uint8_t b4 = (tms9918->palWriteStage0Value & 0x07) << 1;
    uint8_t g4 = (data & 0x07) << 1;
    tms9918->v9938Palette[palIdx] = 0xf000 | ((uint16_t)r4 << 8) | ((uint16_t)g4 << 4) | b4;
    tms9918->palDirty = 1;
    /* R#16 auto-increments after each complete 2-byte write */
    TMS_REGISTER(tms9918, 0x10) = (palIdx + 1) & 0x0f;
  }
}
#endif /* VR_EMU_TMS9918_MODE_V9938 */