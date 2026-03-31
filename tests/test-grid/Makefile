# Makefile: Test grid functions
# Copyright (c) 2026 University of Virginia
# This file is licensed under the MIT License.
# See the LICENSE file in the root of this repository for more details.

PROJ_ROOT = ../..
include ${PROJ_ROOT}/Makefile.common

SRCS := $(sort $(wildcard *.cpp))
EXECS := $(SRCS:.cpp=.out)
OPTIMIZE_LAYOUT_HDR_DIR := $(PROJ_ROOT)/libpimeval/src
OPTIMIZE_LAYOUT_SRC := $(PROJ_ROOT)/libpimeval/src/gridLayoutOptimizer.cpp

debug perf dramsim3_integ: $(EXECS)

test-optimize-layout.out: test-optimize-layout.cpp $(OPTIMIZE_LAYOUT_HDR_DIR)/gridLayoutOptimizer.h $(DEPS)
	$(CXX) $< $(CXXFLAGS) -I$(OPTIMIZE_LAYOUT_HDR_DIR) -o $@

%.out: %.cpp $(DEPS)
	$(CXX) $< $(CXXFLAGS) -o $@

clean:
	rm -rf $(EXECS) *.dSYM

