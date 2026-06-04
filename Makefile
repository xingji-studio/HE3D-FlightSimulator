# HE3D Engine for 3D - Makefile
# Compiler: XJ380 XACT xxcc (requires Clang 18.1.8+ backend)
# Note: xxcc -c has a known crash bug; compile+link in one step.

CXX      := /home/bnear8273/Develop/XJ380_XACT_2026v4_linux/bin/xxcc
CXXFLAGS := -std=c++11 -O2 -I./src -I./include

.PHONY: all clean rebuild

all: he3d_flight

he3d_flight: src/he3d.cpp src/main.cpp src/he3d.hpp src/he3d_math.h
	$(CXX) src/he3d.cpp src/main.cpp -o $@ $(CXXFLAGS)

clean:
	rm -rf xxcc-temp-out he3d_flight he3d_flight.exe

rebuild: clean all
