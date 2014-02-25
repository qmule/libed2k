LIB=libed2k.a

CXXFLAGS = -I../include
OBJDIR=obj
SRC_DIR=. kademlia
SRC := $(foreach sdir,$(SRC_DIR),$(wildcard $(sdir)/*.cpp))
OBJF = $(patsubst %.cpp,$(OBJDIR)/%.o, $(SRC))

$(LIB): $(OBJF)
	$(AR) $(ARFLAGS) $@ $^
	mkdir -p ../lib
	cp $@ ../lib

clean:
	rm -f $(OBJF) $(LIB)
	rm -rf ../lib

include ../Makefile.conf

# EOF #

