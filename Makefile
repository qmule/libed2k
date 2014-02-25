
CXXFLAGS = -Iinclude
OBJF = $(patsubst %.cpp,%.o,$(wildcard $(addsuffix /*.cpp,src)))

.PHONY: all clean libed2k.a test unit


all: libed2k.a test unit

libed2k.a:
	cd src && $(MAKE) $<

test:
	cd test && $(MAKE) all

unit:
	cd unit && $(MAKE) all

clean:
	cd src && $(MAKE) clean
	cd test && $(MAKE) clean
	cd unit && $(MAKE) clean

include Makefile.conf

# EOF #

