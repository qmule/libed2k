EXAMPLE_BIN=crypto

OBJF = ../../src/Crypto.o 
OBJF += $(patsubst %.cpp,%.o,$(wildcard $(addsuffix /*.cpp,.)))

LDFLAGS=-lssl -lboost_iostreams -lboost_thread -lpthread -lboost_system -lboost_signals -lcrypto++ -lcryptopp
CXXFLAGS = -Wall -g -I../../include

include ../../Makefile.conf

# EOF #

