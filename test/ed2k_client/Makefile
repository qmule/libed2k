BIN=ed2k_client
OBJF += $(patsubst %.cpp,%.o,$(wildcard $(addsuffix /*.cpp,.)))
LIBED2K = ../../src/libed2k.a

CXXFLAGS = -g -I../../include
LDFLAGS = -lboost_program_options  -lboost_filesystem


.PHONY: all clean

all: $(BIN)

clean: 
	rm -f $(OBJF)
	rm -f $(BIN)

include ../../Makefile.conf

# EOF #

