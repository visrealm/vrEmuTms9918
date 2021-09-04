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

#if _EMSCRIPTEN
#include <emscripten.h>
#define VR_EMU_TMS9918A_DLLEXPORT EMSCRIPTEN_KEEPALIVE
#elif COMPILING_DLL
#define VR_EMU_TMS9918A_DLLEXPORT __declspec(dllexport)
#else
#define VR_EMU_TMS9918A_DLLEXPORT
//__declspec(dllimport)
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
 * write data (mode = 0) to the tms9918a
 *
 * byte: the data (DB0 -> DB7) to send
 */
VR_EMU_TMS9918A_DLLEXPORT
byte vrEmuTms9918aReadData(VrEmuTms9918a* tms9918a);


/* Function:  vrEmuTms9918aScanLine
 * ----------------------------------------
 * generate a scanline
 *
 * pixels to be filled with TMS9918 color palette indexes (vrEmuTms9918aColor)
 */
VR_EMU_TMS9918A_DLLEXPORT
void vrEmuTms9918aScanLine(VrEmuTms9918a* tms9918a, byte y, byte pixels[TMS9918A_PIXELS_X]);

#endif // _VR_EMU_TMS9918A_CORE_H_