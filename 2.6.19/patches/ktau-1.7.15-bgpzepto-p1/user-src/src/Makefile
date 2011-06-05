#****************************************************************************
#*			KTAU User-Library & Read/Write Tools 		   **
#****************************************************************************

include Make.rules
 
ARCH            = $(shell uname -m)

#Architecture Declaration for KTAUD
ifeq ($(ARCH), x86_64)
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
KTAU_ROOT	= ../../

KTAU_INCLUDE	= ../include

KTAU_LIBS	= ../lib

KTAU_BIN	= ../bin

KTAU_FLAGS	= KTAU_USER_SRC_COMPILE

KERNEL_SRC	= $(shell cd ../../src/; pwd)
#KERNEL_SRC	= /lib/modules/$(shell uname -r)/build

#CXX		= g++
#CC		= gcc

CFLAGS		= -g -c -I$(KERNEL_SRC)/include -I$(KTAU_INCLUDE) -I$(BGP_KERNEL_INC) -D$(KTAU_FLAGS) -D$(KTAU_ARCH)

LDFLAGS	      	= -L$(KTAU_LIBS) -lktau

TARGET		= ktau_diff_profiles.o ktau_timer.o ktau_proc_interface.o ktau_proc_map.o ktau_and_tau.o libktau.a runktau
#tools ktaud


all:	$(TARGET)	

libktau.a: ktau_proc_interface.o
	mkdir -p ../lib/
	ar cqs ../lib/libktau.a ktau_timer.o ktau_proc_interface.o ktau_proc_map.o ktau_and_tau.o ktau_diff_profiles.o

.PHONY: ktau_timer.o
ktau_timer.o:
	$(CC) $(CFLAGS) ktau_timer.c

.PHONY: ktau_proc_interface.o
ktau_proc_interface.o:	
	$(CC) $(CFLAGS) ktau_proc_interface.c

.PHONY: ktau_diff_profiles.o
ktau_diff_profiles.o:	
	$(CC) $(CFLAGS) ktau_diff_profiles.c

.PHONY: ktau_proc_map.o
ktau_proc_map.o:	
	$(CXX) $(CFLAGS) ktau_proc_map.cpp

.PHONY: ktau_and_tau.o
ktau_and_tau.o:	
	$(CXX) $(CFLAGS) ktau_and_tau.cpp

.PHONY: tools
tools:
	make -C tools

.PHONY: ktaud
ktaud: 
	make -C ktaud

.PHONY: runktau
runktau: 
	make -C runktau

install: all
	mkdir -p $(KTAU_BIN)
	make -C ktaud install
	make -C tools install
	make -C runktau install
	make -C tests install

clean:
	make -C ktaud clean
	make -C tools clean
	make -C runktau clean
	make -C tests clean
	rm -rf ../lib/libktau.a
	rm -rf ktau_proc_interface.o
	rm -rf ktau_and_tau.o  ktau_diff_profiles.o  ktau_proc_map.o  ktau_timer.o

