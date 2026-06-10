# Makefile: C++ version of stencil on CPU
# Copyright (c) 2026 University of Virginia
# This file is licensed under the MIT License.
# See the LICENSE file in the root of this repository for more details.

CXX := g++

CXXFLAGS := -Wall -Wextra -Werror -std=c++20 -O3 -fopenmp

# Executable name
EXEC := stencil.out

# Source files
SRC_FILES := $(wildcard *.cpp)

#Util Include
INCLUDE_FLAGS := -I../../../../util


.PHONY: all clean

all: $(EXEC)

$(EXEC): $(SRC_FILES) |
	$(CXX) $(CXXFLAGS) $(INCLUDE_FLAGS) -o $@ $^

clean:
	rm -rf $(EXEC)
