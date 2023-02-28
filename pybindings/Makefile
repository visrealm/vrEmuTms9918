CFLAGS= -D VR_TMS9918_EMU_STATIC -I ../src
CXXFLAGS=-O3 -Wall $(CFLAGS)
PYTHON=python3

%.o: ../src/%.c
	cc $(CFLAGS) -c $@ $<

tms9918:vrEmuTms9918.o  vrEmuTms9918Util.o
	g++ $(OPT) -Wall -shared -std=c++11 -fPIC $(CXXFLAGS) `$(PYTHON) -m pybind11 --includes` $@.cpp  vrEmuTms9918.o  vrEmuTms9918Util.o -o $@`$(PYTHON)-config --extension-suffix`


clean:
	rm *.so *.o
