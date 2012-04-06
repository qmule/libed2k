EXAMPLE_BIN=https

OBJF = ../../src/is_https_auth.o 
OBJF += $(patsubst %.cpp,%.o,$(wildcard $(addsuffix /*.cpp,.)))

LDFLAGS=-lssl -lboost_iostreams -lboost_thread -lpthread -lboost_system -lboost_signals -lcrypto++ -lcryptopp
CXXFLAGS = -Wall -g -I../../include

include ../../Makefile.conf

# EOF #

