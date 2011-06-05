#****************************************************************************
#*			KTAU User-Library & Read/Write Tools 		   **
#*					runktau				   **
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
#CXX		= g++

CFLAGS          = -g -c -I$(KERNEL_SRC)/include -I$(KTAU_INCLUDE) -D$(KTAU_FLAGS) -D$(KTAU_ARCH)

LDFLAGS	      	= -L$(KTAU_LIBS) -lktau -lstdc++

TARGET		= timeKtau

all:	$(TARGET)	

timeKtau: timeKtau.o
	$(CC) -o timeKtau timeKtau.o $(LDFLAGS)
#	$(LD) -o timeKtau timeKtau.o $(LDFLAGS)

timeKtau.o:	
	$(CC) $(CFLAGS) timeKtau.c

runktauTrace: runktauTrace.o
	$(CC) -o runktauTrace runktauTrace.o $(LDFLAGS)

runktauTrace.o:	
	$(CC) $(CFLAGS) runktauTrace.c

runktau: runktau.o
	$(CC) -o runktau runktau.o $(LDFLAGS)

runktau.o:	
	$(CC) $(CFLAGS) runktau.c

install: all
	mkdir -p $(KTAU_BIN)
	cp timeKtau $(KTAU_BIN)/

clean:
	rm -rf runktau.o runktau runktauTrace runktauTrace.o timeKtau timeKtau.o daemon
	rm -rf ${KTAU_BIN}/timeKtau
