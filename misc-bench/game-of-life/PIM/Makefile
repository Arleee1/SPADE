# Makefile: C++ version of game of life
# Copyright (c) 2024 University of Virginia
# This file is licensed under the MIT License.
# See the LICENSE file in the root of this repository for more details.

PROJ_ROOT = ../../..
include ${PROJ_ROOT}/Makefile.common

# make USE_OPENMP=1
USE_OPENMP ?= 0
ifeq ($(USE_OPENMP),1)
	CXXFLAGS += -fopenmp
endif

SRCS := game-of-life-grid.cpp game-of-life.cpp
EXECS := $(SRCS:.cpp=.out)

debug perf dramsim3_integ: $(EXECS)

%.out: %.cpp $(DEPS)
	$(CXX) $< $(CXXFLAGS) -o $@

clean:
	rm -rf $(EXECS) *.dSYM