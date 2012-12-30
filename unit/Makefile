BIN=run_tests


LIBED2K = ../src/libed2k.a 
OBJF += $(patsubst %.cpp,%.o,$(wildcard $(addsuffix /*.cpp,.)))

LDFLAGS=-lboost_unit_test_framework -lboost_filesystem -L../src -led2k
CXXFLAGS = -I../include -ldl

.PHONY: all clean

all: $(BIN)

clean:
		rm -f $(OBJF)
		rm -f $(BIN)

include ../Makefile.conf

# EOF #

