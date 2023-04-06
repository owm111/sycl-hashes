sources := hash_benchmark.cpp sha224.cpp
program := hash_benchmark
objects := $(sources:cpp=o)
depends := $(sources:cpp=d)

CC = icx
CFLAGS := -fsycl -Rno-debug-disables-optimization
CXX := icpx
CXXFLAGS := -fsycl -std=c++17 -Wall -g -Rno-debug-disables-optimization
CPPFLAGS := \
	-I/opt/intel/oneapi/compiler/latest/linux/include \
	-I/opt/intel/oneapi/compiler/latest/linux/include/sycl
LDLIBS := -lstdc++
LDFLAGS := -fsycl

all: $(program)

clean:
	$(RM) $(program) $(depends) $(objects)

.PHONY: all clean

$(program): $(objects)

%.d: %.cpp
	$(CXX) -MM $(CPPFLAGS) $< | sed 's,$*.o[ :]*,$*.o $@ : ,g' >$@

include $(depends)
