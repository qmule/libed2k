LIB=libed2k.a

CXXFLAGS = -I../include
OBJF = $(patsubst %.cpp,%.o,$(wildcard $(addsuffix /*.cpp,.)))
OBJF += $(patsubst %.c,%.o,$(wildcard $(addsuffix /*.c,.)))

$(LIB): $(OBJF)
	$(AR) $(ARFLAGS) $@ $^
	mkdir -p ../lib
	cp $@ ../lib

clean:
	rm -f $(OBJF) $(LIB)
	rm -rf ../lib

include ../Makefile.conf

# EOF #

