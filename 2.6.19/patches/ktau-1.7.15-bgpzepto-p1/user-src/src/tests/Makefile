#****************************************************************************
#*			KTAU Tests			 		   **
#*									   **
#****************************************************************************
 
include ../Make.rules

ARCH            = $(shell uname -m)

#Architecture Declaration for KTAUD
ifeq ($(ARCH),x86_64)
        KTAU_ARCH = KTAUD_x86_64
endif
ifeq ($(ARCH), ppc)
        KTAU_ARCH = KTAUD_PPC
endif
ifeq ($(ARCH),ppc64)
        KTAU_ARCH = KTAUD_PPC64
endif
ifeq ($(ARCH),i386)
        KTAU_ARCH = KTAUD_PENTIUM
endif
ifeq ($(ARCH),i486)
        KTAU_ARCH = KTAUD_PENTIUM
endif
ifeq ($(ARCH),i686)
        KTAU_ARCH = KTAUD_PENTIUM
endif
ifeq ($(ARCH),mips64)
        KTAU_ARCH = KTAUD_MIPS64
endif

##############################################
KTAU_ROOT	= ../../../

KTAU_INCLUDE	= ../../include

KTAU_LIBS	= ../../lib

KTAU_BIN	= ../../bin

KTAU_FLAGS      = KTAU_USER_SRC_COMPILE

KERNEL_SRC      = $(shell cd ../../../src/; pwd)
#KERNEL_SRC      = /lib/modules/$(shell uname -r)/build

#CC		= gcc
#CPP		= g++

CFLAGS          = -g -c -I$(KERNEL_SRC)/include -I$(KTAU_INCLUDE) -I$(BGP_KERNEL_INC) -D$(KTAU_FLAGS) -D$(KTAU_ARCH)

PAPIFLAGS	= -I/usr/local/packages/papi/include/ -L/usr/local/packages/papi/lib/

LDFLAGS	      	= -L$(KTAU_LIBS) -lktau -lstdc++

TARGET		= shcont shctr timing inject timepapi

all:	$(TARGET)	

timing: timing.o
	$(CC) -O0 -o timing timing.o ../ktau_timer.o $(LDFLAGS)

timing.o:	
	$(CC) -O0 $(CFLAGS) timing.c

shcont: shcont.o
	$(CC) -o shcont shcont.o $(LDFLAGS)

shcont.o:	
	$(CC) $(CFLAGS) shcont.c

shctr: shctr.o
	$(CC) -o shctr shctr.o $(LDFLAGS)

shctr.o:	
	$(CC) $(CFLAGS) shctr.c

inject: inject.o
	$(CC) -O0 -o inject inject.o ../ktau_timer.o $(LDFLAGS)

inject.o:	
	$(CC) -O0 $(CFLAGS) inject.c

timepapi: timepapi.o
	$(CC) -O0 $(PAPIFLAGS) -o timepapi timepapi.o ../ktau_timer.o $(LDFLAGS) /usr/local/packages/papi/lib/libpapi.a

timepapi.o:	
	$(CC) -O0 $(PAPIFLAGS) $(CFLAGS) timepapi.c 

install: all
	mkdir -p $(KTAU_BIN)
	mkdir -p $(KTAU_BIN)/tests
	cp shcont shctr timing inject $(KTAU_BIN)/tests/

clean:
	rm -rf shcont.o shcont shctr shctr.o timing timing.o inject inject.o timepapi timepapi.o
	rm -rf ${KTAU_BIN}/tests/shcont ${KTAU_BIN}/tests/shctr ${KTAU_BIN}/tests/timing ${KTAU_BIN}/tests/inject
