EXAMPLE_BIN=run_tests


BOOST_HOME=/home/apavlov/local
OBJF = ../src/is_crypto.o ../src/error_code.o 
OBJF += $(patsubst %.cpp,%.o,$(wildcard $(addsuffix /*.cpp,.)))

LDFLAGS=-L$(BOOST_HOME)/lib -lssl -lboost_iostreams -lboost_thread -lpthread -lboost_system -lboost_signals -lboost_filesystem -lboost_unit_test_framework -lcrypto++ -lcryptopp
CXXFLAGS = -I$(BOOST_HOME)/include -Wall -g -I../include

include ../Makefile.conf

# EOF #

