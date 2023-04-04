sources := hash_benchmark.cpp sha224.cpp
program := hash_benchmark
objects := $(sources:cpp=o)
depends := $(sources:cpp=d)

CXX := g++
CXXFLAGS := -std=c++17 -Wall -g
CPPFLAGS :=
LDLIBS := -lstdc++
LDFLAGS :=

all: $(program)

clean:
	$(RM) $(program) $(depends)

.PHONY: all clean

$(program): $(objects)

%.d: %.cpp
	$(CXX) -MM $(CPPFLAGS) $< | sed 's,$*.o[ :]*,$*.o $@ : ,g' >$@

include $(depends)
