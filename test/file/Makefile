BIN=file

OBJF += $(patsubst %.cpp,%.o,$(wildcard $(addsuffix /*.cpp,.)))
LIBED2K = ../../src/libed2k.a -lboost_filesystem
CXXFLAGS = -g -I../../include

.PHONY: all clean

all: $(BIN)

clean: 
	rm -f $(OBJF)
	rm -f $(BIN)

include ../../Makefile.conf

# EOF #

