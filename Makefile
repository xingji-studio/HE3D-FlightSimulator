# HE3D Flight Simulator - Makefile
# Compiler: XJ380 XACT xxcc (requires Clang 18.1.8+ backend)
# Note: xxcc -c has a known crash bug; compile+link in one step.

CXX      := xxcc
CXXFLAGS := -std=c++11 -O2 -I./src -I./include

.PHONY: all clean rebuild

all: he3d_flight

he3d_flight: src/he3d.cpp src/flight_sim.cpp src/main.cpp src/he3d.hpp src/he3d_math.h src/flight_sim.hpp
	$(CXX) src/he3d.cpp src/flight_sim.cpp src/main.cpp -o $@ $(CXXFLAGS)

clean:
	rm -rf xxcc-temp-out he3d_flight he3d_flight.exe

rebuild: clean all
