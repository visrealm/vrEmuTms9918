#include "vrEmuTms9918Util.h"
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/stl_bind.h>
#include <stdint.h>
#include <vector>

class Tms9918 {
public:
  Tms9918();
  ~Tms9918();
  void setReg(uint8_t reg, uint8_t val);
  void setRegs(const std::vector<uint8_t> &val);
  void setVram(uint16_t addr, const std::vector<uint8_t> &data);
  std::vector<uint8_t> getScreen();

private:
  VrEmuTms9918 *t;
};

Tms9918::Tms9918() { t = vrEmuTms9918New(); }

Tms9918::~Tms9918() { vrEmuTms9918Destroy(t); }

void Tms9918::setReg(uint8_t reg, uint8_t val) {
  vrEmuTms9918WriteRegValue(t, vrEmuTms9918Register(reg), val);
}

void Tms9918::setRegs(const std::vector<uint8_t> &val) {
  for (size_t i = 0; i < val.size(); ++i) {
    vrEmuTms9918WriteRegValue(t, vrEmuTms9918Register(i), val[i]);
  }
}

void Tms9918::setVram(uint16_t addr, const std::vector<uint8_t> &data) {
  vrEmuTms9918SetAddressWrite(t, addr);
  vrEmuTms9918WriteBytes(t, data.data(), data.size());
}

std::vector<uint8_t> Tms9918::getScreen() {
  uint8_t scanline[TMS9918_PIXELS_X]; // scanline buffer

  // an example output (a framebuffer for an SDL texture)
  std::vector<uint8_t> framebuffer(TMS9918_PIXELS_X * TMS9918_PIXELS_Y * 3);

  // generate all scanlines and render to framebuffer
  size_t c = 0;
  for (int y = 0; y < TMS9918_PIXELS_Y; ++y) {
    // get the scanline pixels
    vrEmuTms9918ScanLine(t, y, scanline);
    for (int x = 0; x < TMS9918_PIXELS_X; ++x) {
      // values returned from vrEmuTms9918ScanLine() are palette indexes
      // use the vrEmuTms9918Palette array to convert to an RGBA value

      uint32_t col = vrEmuTms9918Palette[scanline[x]];
      framebuffer[c++] = (col >> 24) & 0xFF;
      framebuffer[c++] = (col >> 16) & 0xFF;
      framebuffer[c++] = (col >> 8) & 0xFF;
    }
  }
  return framebuffer;
}

namespace py = pybind11;

PYBIND11_MODULE(tms9918, m) {
  m.doc() = "Tms9918"; // optional module docstring
  py::class_<Tms9918>(m, "Tms9918")
      .def(py::init<>())
      .def("setReg", &Tms9918::setReg)
      .def("setRegs", &Tms9918::setRegs)
      .def("setVram", &Tms9918::setVram)
      .def("getScreen", &Tms9918::getScreen);
}
