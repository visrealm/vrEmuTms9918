# vrEmuTms9918 - TMS9918 / TMS9918A / TMS9929A VDP Emulator

TMS9918 emulator. Core engine written in C99. Zero dependencies.

The goal is to emulate all documented modes listed in the [TMS9918A/TMS9928A/TMS9929A datasheet](http://www1.cs.columbia.edu/~sedwards/papers/TMS9918.pdf)

## Supported Modes

* Graphics I (including sprites)
* Graphics II (including sprites)
* Multicolor mode (including sprites)
* Text

## Other features

* 5th sprite
* Sprite collisions
* VSYNC interrupt
* Individual scanline rendering

## Screenshots:

#### Graphics Mode I Demo
<img src="res/mode1demo.gif" alt="Graphics Mode I Demo" width="1279px">

#### Graphics Mode II Demo
<img src="res/mode2demo.gif" alt="Graphics Mode II Demo" width="1279px">

#### Text Mode Demo
<img src="res/textdemo.gif" alt="Text Mode Demo" width="1279px">

#### Multicolor Mode Demo
<img src="res/mcdemo.gif" alt="Multicolor Mode Demo" width="1279px">


## Quick start

```c
#include "vrEmuTms9918.h"
#include "vrEmuTms9918Util.h"

#define TMS_VRAM_NAME_ADDRESS          0x3800
#define TMS_VRAM_COLOR_ADDRESS         0x0000
#define TMS_VRAM_PATT_ADDRESS          0x2000
#define TMS_VRAM_SPRITE_ATTR_ADDRESS   0x3B00
#define TMS_VRAM_SPRITE_PATT_ADDRESS   0x1800

VrEmuTms9918 *tms9918 = NULL;

// program entry point
int main()
{
  // create a new tms9918
  tms9918 = vrEmuTms9918New();
  
  // Here, we're using the helper functions provided by vrEmuTms9918Util.h
  //
  // In a full system emulator, the only functions required (connected to the system bus) would be:
  //
  //  * vrEmuTms9918WriteAddr()
  //  * vrEmuTms9918WriteData()
  //  * vrEmuTms9918ReadStatus()
  //  * vrEmuTms9918ReadData()
  //
  // The helper functions below wrap the above functions and are not required.
  // vrEmuTms9918Util.h/c can be omitted if you're not using them.
  
  // set up the VDP write-only registers
  vrEmuTms9918WriteRegisterValue(tms9918, TMS_REG_0, TMS_R0_MODE_GRAPHICS_I);
  vrEmuTms9918WriteRegisterValue(tms9918, TMS_REG_1, TMS_R1_MODE_GRAPHICS_I | TMS_R1_RAM_16K);
  vrEmuTms9918SetNameTableAddr(tms9918, TMS_VRAM_NAME_ADDRESS);
  vrEmuTms9918SetColorTableAddr(tms9918, TMS_VRAM_COLOR_ADDRESS);
  vrEmuTms9918SetPatternTableAddr(tms9918, TMS_VRAM_PATT_ADDRESS);
  vrEmuTms9918SetSpriteAttrTableAddr(tms9918, TMS_VRAM_SPRITE_ATTR_ADDRESS);
  vrEmuTms9918SetSpritePattTableAddr(tms9918, TMS_VRAM_SPRITE_PATT_ADDRESS);
  vrEmuTms9918SetFgBgColor(tms9918, TMS_BLACK, TMS_CYAN);
  
  // send it some data (a pattern)
  vrEmuTms9918SetAddressWrite(tms9918, TMS_VRAM_PATT_ADDRESS);
  
  char smile[] = { 0x3C, 0x42, 0x81, 0xA5, 0x81, 0x99, 0x42, 0x3C };
  vrEmuTms9918WriteBytes(tms9918, smile, sizeof(smile));
  
  vrEmuTms9918SetAddressWrite(tms9918, TMS_VRAM_COLOR_ADDRESS)
  vrEmuTms9918WriteData(tms9918, vrEmuTms9918FgBgColor(TMS_BLACK, TMS_LT_YELLOW));
  

  // send a byte to the name table
  vrEmuTms9918SetAddressWrite(tms9918, TMS_VRAM_NAME_ADDRESS);

  // a few smiles
  vrEmuTms9918WriteData(tms9918, 0x00);
  vrEmuTms9918WriteData(tms9918, 0x00);
  vrEmuTms9918WriteData(tms9918, 0x00);
  
  char scanline[TMS9918A_PIXELS_X]; // scanline buffer
  
  uint32_t frameBuffer[TMS9918A_PIXELS_X * TMS9918A_PIXELS_Y]; // framebuffer (for SDL texture)

  // render the display
  uint32_t *pixPtr = frameBuffer;
  for (int y = 0; y < TMS9918A_PIXELS_Y; ++y)
  {
    // get the scanline pixels
    vrEmuTms9918ScanLine(tms9918, y, scanline);
    
    // here, you can do whatever you like with the pixel data
    // eg. render to an SDL texture/framebuffer...
    for (int x = 0; x < TMS9918A_PIXELS_X; ++x)
    {
      *pixPtr++ = vrEmuTms9918Palette[scanline[x]];
    }    
  }
  
  // output the buffer...
  
  vrEmuTms9918Destroy(tms9918);
  tms9918 = NULL;
  
  return 0;
}
```

## License
This code is licensed under the [MIT](https://opensource.org/licenses/MIT "MIT") license
