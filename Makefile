sources := hash_benchmark.cpp sha224.cpp
program := hash_benchmark
objects := $(sources:cpp=o)
depends := $(sources:cpp=d)

CC = icx
CFLAGS := -Rno-debug-disables-optimization
CXX := icpx
CXXFLAGS := -std=c++17 -Wall -g -Rno-debug-disables-optimization
CPPFLAGS :=
LDLIBS := -lstdc++
LDFLAGS :=

all: $(program)

clean:
	$(RM) $(program) $(depends) $(objects)

.PHONY: all clean

$(program): $(objects)

%.d: %.cpp
	$(CXX) -MM $(CPPFLAGS) $< | sed 's,$*.o[ :]*,$*.o $@ : ,g' >$@

include $(depends)
