# Makefile: Extra stencil experiments
# Copyright (c) 2026 University of Virginia
# This file is licensed under the MIT License.
# See the LICENSE file in the root of this repository for more details.

PROJ_ROOT = ../../..
include ${PROJ_ROOT}/Makefile.common

DEPS += $(UTIL_DIR)/utilGrid.h

SRCS := test_partition.cpp
EXECS := $(SRCS:.cpp=.out)

debug perf dramsim3_integ: $(EXECS)

%.out: %.cpp $(DEPS)
	$(CXX) $< $(CXXFLAGS) -o $@

run: perf
	./test_partition.out

clean:
	rm -rf $(EXECS) *.dSYM

.PHONY: run