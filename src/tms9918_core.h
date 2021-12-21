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

#ifndef _VR_EMU_TMS9918A_CORE_H_
#define _VR_EMU_TMS9918A_CORE_H_

#if VR_TMS9918_EMU_COMPILING_DLL
#define VR_EMU_TMS9918A_DLLEXPORT __declspec(dllexport)
#elif VR_TMS9918_EMU_STATIC
#define VR_EMU_TMS9918A_DLLEXPORT extern
#elif _EMSCRIPTEN
#include <emscripten.h>
#define VR_EMU_TMS9918A_DLLEXPORT EMSCRIPTEN_KEEPALIVE
#else
#define VR_EMU_TMS9918A_DLLEXPORT __declspec(dllimport)
#endif

#undef byte
typedef unsigned char byte;

/* PRIVATE DATA STRUCTURE
 * ---------------------------------------- */
struct vrEmuTMS9918a_s;
typedef struct vrEmuTMS9918a_s VrEmuTms9918a;

typedef enum
{
  TMS_MODE_GRAPHICS_I,
  TMS_MODE_GRAPHICS_II,
  TMS_MODE_TEXT,
  TMS_MODE_MULTICOLOR,
} vrEmuTms9918aMode;

typedef enum
{
  TMS_TRANSPARENT,
  TMS_BLACK,
  TMS_MED_GREEN,
  TMS_LT_GREEN,
  TMS_DK_BLUE,
  TMS_LT_BLUE,
  TMS_DK_RED,
  TMS_CYAN,
  TMS_MED_RED,
  TMS_LT_RED,
  TMS_DK_YELLOW,
  TMS_LT_YELLOW,
  TMS_DK_GREEN,
  TMS_MAGENTA,
  TMS_GREY,
  TMS_WHITE,
} vrEmuTms9918aColor;

typedef enum
{
  TMS_REG_0,
  TMS_REG_1,
  TMS_REG_2,
  TMS_REG_3,
  TMS_REG_4,
  TMS_REG_5,
  TMS_REG_6,
  TMS_REG_7,
  TMS_NUM_REGISTERS
} vrEmuTms9918aRegister;

#define TMS9918A_PIXELS_X 256
#define TMS9918A_PIXELS_Y 192


/* PUBLIC INTERFACE
 * ---------------------------------------- */

 /* Function:  vrEmuTms9918aNew
  * --------------------
  * create a new TMS9918A
  */
VR_EMU_TMS9918A_DLLEXPORT
VrEmuTms9918a* vrEmuTms9918aNew();

/* Function:  vrEmuTms9918aReset
  * --------------------
  * reset the new TMS9918A
  */
VR_EMU_TMS9918A_DLLEXPORT
void vrEmuTms9918aReset(VrEmuTms9918a* tms9918a);

/* Function:  vrEmuTms9918aDestroy
 * --------------------
 * destroy a TMS9918A
 *
 * tms9918a: tms9918a object to destroy / clean up
 */
VR_EMU_TMS9918A_DLLEXPORT
void vrEmuTms9918aDestroy(VrEmuTms9918a* tms9918a);

/* Function:  vrEmuTms9918aWriteAddr
 * --------------------
 * write an address (mode = 1) to the tms9918a
 *
 * byte: the data (DB0 -> DB7) to send
 */
VR_EMU_TMS9918A_DLLEXPORT
void vrEmuTms9918aWriteAddr(VrEmuTms9918a* tms9918a, byte data);

/* Function:  vrEmuTms9918aWriteData
 * --------------------
 * write data (mode = 0) to the tms9918a
 *
 * byte: the data (DB0 -> DB7) to send
 */
VR_EMU_TMS9918A_DLLEXPORT
void vrEmuTms9918aWriteData(VrEmuTms9918a* tms9918a, byte data);

/* Function:  vrEmuTms9918aReadStatus
 * --------------------
 * read from the status register
 */
VR_EMU_TMS9918A_DLLEXPORT
byte vrEmuTms9918aReadStatus(VrEmuTms9918a* tms9918a);

/* Function:  vrEmuTms9918aReadData
 * --------------------
 * read data (mode = 0) from the tms9918a
 */
VR_EMU_TMS9918A_DLLEXPORT
byte vrEmuTms9918aReadData(VrEmuTms9918a* tms9918a);

/* Function:  vrEmuTms9918aReadDataNoInc
 * --------------------
 * read data (mode = 0) from the tms9918a
 * don't increment the address pointer
 */
VR_EMU_TMS9918A_DLLEXPORT
byte vrEmuTms9918aReadDataNoInc(VrEmuTms9918a* tms9918a);


/* Function:  vrEmuTms9918aScanLine
 * ----------------------------------------
 * generate a scanline
 *
 * pixels to be filled with TMS9918 color palette indexes (vrEmuTms9918aColor)
 */
VR_EMU_TMS9918A_DLLEXPORT
void vrEmuTms9918aScanLine(VrEmuTms9918a* tms9918a, byte y, byte pixels[TMS9918A_PIXELS_X]);

/* Function:  vrEmuTms9918aRegValue
 * ----------------------------------------
 * return a reigister value
 */
VR_EMU_TMS9918A_DLLEXPORT
byte vrEmuTms9918aRegValue(VrEmuTms9918a* tms9918a, vrEmuTms9918aRegister reg);

/* Function:  vrEmuTms9918aVramValue
 * ----------------------------------------
 * return a value from vram
 */
VR_EMU_TMS9918A_DLLEXPORT
byte vrEmuTms9918aVramValue(VrEmuTms9918a* tms9918a, unsigned short addr);


/* Function:  vrEmuTms9918aDisplayEnabled
  * --------------------
  * check BLANK flag
  */
VR_EMU_TMS9918A_DLLEXPORT
int vrEmuTms9918aDisplayEnabled(VrEmuTms9918a* tms9918a);


#endif // _VR_EMU_TMS9918A_CORE_H_
