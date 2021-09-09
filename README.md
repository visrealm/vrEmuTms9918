# vrEmuTms9918 - TMS9918A/TMS9928A/TMS9929A VDP Emulator

Core engine written in C.

The goal is to emulate all documented modes listed in the [TMS9918A/TMS9928A/TMS9929A datasheet](http://www1.cs.columbia.edu/~sedwards/papers/TMS9918.pdf)

## Supported Modes

* Graphics I (including sprites)
* Graphics II (including sprites)
* Text

## Current limitations

* Multicolor mode is not implemented
* Sprite collisions not handled
* Will output more than 4 sprites on a scanline

## Usage

The core API is used to produce an image one scanline at a time.

## Screenshots:

#### Graphics Mode I Demo
<img src="res/mode1demo.gif" alt="Graphics Mode I Demo" width="1279px">

#### Graphics Mode II Demo
<img src="res/mode2demo.gif" alt="Graphics Mode II Demo" width="1279px">
