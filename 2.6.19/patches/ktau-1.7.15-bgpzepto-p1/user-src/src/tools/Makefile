#****************************************************************************
#*			KTAU User-Library & Read/Write Tools 		   **
#****************************************************************************
 
KTAU_ROOT	= ../../../

KTAU_INCLUDE	= ../../include

KTAU_LIBS	= ../../lib

KTAU_BIN	= ../../bin
	
KTAU_FLAGS	= KTAU_USER_SRC_COMPILE

#KERNEL_SRC	= $(shell cd ../../../src/; pwd)
KERNEL_SRC	= /lib/modules/$(shell uname -r)/build

CXX		= g++

CC		= gcc

CFLAGS		= -g -c -I$(KERNEL_SRC)/include -I$(KTAU_INCLUDE) -D$(KTAU_FLAGS) 

LDFLAGS	      	= -L$(KTAU_LIBS) -lktau

TARGET		= TAU_trace.o ktau_convert ktau_convert_fix funcMap ktau2paraprof

##############################################

all:	$(TARGET)	

ktau_convert: ktau_convert.o TAU_trace.o
	$(CXX) -o ktau_convert ktau_convert.o TAU_trace.o

ktau_convert_fix: ktau_convert_fix.o TAU_trace.o
	$(CXX) -o ktau_convert_fix ktau_convert_fix.o TAU_trace.o

TAU_trace.o: TAU_trace.c
	$(CC) $(CFLAGS) TAU_trace.c

ktau_convert.o: ktau_convert.cpp
	$(CXX) $(CFLAGS) ktau_convert.cpp

ktau_convert_fix.o: ktau_convert_fix.cpp
	$(CXX) $(CFLAGS) ktau_convert_fix.cpp

funcMap: funcMap.o
	$(CXX) -o funcMap funcMap.o

funcMap.o:
	$(CXX) $(CFLAGS) funcMap.cpp

ktau2paraprof: ktau2paraprof.o
	$(CXX) -o ktau2paraprof ktau2paraprof.o

ktau2paraprof.o: ktau2paraprof.cpp
	$(CXX) $(CFLAGS) ktau2paraprof.cpp

install: all
	cp ktau2paraprof $(KTAU_BIN)
	cp ktau_convert $(KTAU_BIN)
	cp ktau_convert_fix $(KTAU_BIN)
	cp funcMap $(KTAU_BIN)

clean:
	rm -f *.o
	rm -f $(TARGET)
	rm -f $(KTAU_BIN)/ktau2paraprof
	rm -f $(KTAU_BIN)/ktau_convert
	rm -f $(KTAU_BIN)/ktau_convert_fix
	rm -f $(KTAU_BIN)/funcMap
