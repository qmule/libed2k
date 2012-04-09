LIB=libed2k.a
CXXFLAGS = -I../include
OBJF = $(patsubst %.cpp,%.o,$(wildcard $(addsuffix /*.cpp,.)))

$(LIB): $(OBJF)
	$(AR) $(ARFLAGS) $@ $^

clean:
	rm -f $(OBJF) $(LIB)

include ../Makefile.conf

# EOF #

