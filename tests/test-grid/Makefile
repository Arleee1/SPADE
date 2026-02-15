# Makefile: Test grid functions
# Copyright (c) 2026 University of Virginia
# This file is licensed under the MIT License.
# See the LICENSE file in the root of this repository for more details.

PROJ_ROOT = ../..
include ${PROJ_ROOT}/Makefile.common

EXEC := test-grid.out
SRC := test-grid.cpp

debug perf dramsim3_integ: $(EXEC)

$(EXEC): $(SRC) $(DEPS)
	$(CXX) $< $(CXXFLAGS) -o $@

clean:
	rm -rf $(EXEC) *.dSYM

