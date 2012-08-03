ifdef PRODUCTION
LIB=libed2k.a
else 
LIB=libed2kd.a
endif

CXXFLAGS = -I../include
OBJF = $(patsubst %.cpp,%.o,$(wildcard $(addsuffix /*.cpp,.)))

$(LIB): $(OBJF)
	$(AR) $(ARFLAGS) $@ $^
	mkdir -p ../lib
	cp $@ ../lib

clean:
	rm -f $(OBJF) $(LIB)
	rm -rf ../lib

include ../Makefile.conf

# EOF #

