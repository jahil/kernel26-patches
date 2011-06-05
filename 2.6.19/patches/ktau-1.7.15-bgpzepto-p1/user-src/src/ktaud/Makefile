#****************************************************************************
#*			KTAU User-Library & Read/Write Tools 		   **
#****************************************************************************
 
include ../Make.rules

KTAU_ROOT	= ../../../

KTAU_INCLUDE	= ../../include

KTAU_LIBS	= ../../lib

KTAU_BIN	= ../../bin

KTAU_FLAGS	= KTAU_USER_SRC_COMPILE

KERNEL_SRC      = $(shell cd ../../../src/; pwd)
#KERNEL_SRC	= /lib/modules/$(shell uname -r)/build

CC		= gcc

CFLAGS		= -g -c -I$(KERNEL_SRC)/include -I$(KTAU_INCLUDE) -I$(BGP_KERNEL_INC) -D$(KTAU_FLAGS) -D$(KTAU_ARCH)

LDFLAGS	      	= -L$(KTAU_LIBS) -lktau

TARGET		= ktaud

ARCH		= $(shell uname -m)

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
all:	$(TARGET)	

ktaud: ktaud.o
	$(CC) -o ktaud ktaud.o $(LDFLAGS)

ktaud.o:	
	$(CC) $(CFLAGS) ktaud.c

install: all
	mkdir -p $(KTAU_BIN)
	cp ktaud $(KTAU_BIN)/

clean:
	rm -rf ktaud.o ktaud
	rm -rf ktaud.sh
	rm -rf ktaud.conf
	rm -rf $(KTAU_BIN)/ktaud
