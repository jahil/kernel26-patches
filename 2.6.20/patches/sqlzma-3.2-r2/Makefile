
# Copyright (C) 2006, 2007 Junjiro Okajima
# Copyright (C) 2006, 2007 Tomas Matejicek, slax.org
#
# LICENSE follows the described ones in lzma and squashfs.

# $Id: Makefile,v 1.17 2007/01/16 05:21:32 jro Exp $

# paths
Sqlzma = ${CURDIR}
LzmaVer = lzma443
Lzma = ${Sqlzma}/${LzmaVer}
SqVer = squashfs3.2-r2
Squashfs = ${Sqlzma}/${SqVer}
KVer = linux-2.6.20-rc3
# it should match the name of squashfs dir.
KVer = linux-2.6.20
SqFs = ${Squashfs}/kernel-patches/${KVer}/fs/squashfs
KDir = /lib/modules/$(shell uname -r)/build

LzmaC = ${Lzma}/C/7zip/Compress/LZMA_C
LzmaAlone = ${Lzma}/C/7zip/Compress/LZMA_Alone
SqTools = ${Squashfs}/squashfs-tools

# enable it if you want to add -g option when compiling
UseDebugFlags =
# disable it if you don't want to compile squashfs kernel module here
BuildSquashfs = 1

export

all:
	${MAKE} -C ${LzmaC} -f sqlzma.mk $@
	${MAKE} -C ${LzmaAlone} -f sqlzma.mk $@
	${MAKE} -C ${SqTools} $@

clean:
	${MAKE} -C ${LzmaC} -f sqlzma.mk $@
	${MAKE} -C ${LzmaAlone} -f sqlzma.mk $@
	${MAKE} -C ${SqTools} $@
	${RM} *~

########################################

-include priv.mk

ifdef BuildSquashfs
CONFIG_SQUASHFS = m
CONFIG_SQUASHFS_EMBEDDED =
CONFIG_SQUASHFS_FRAGMENT_CACHE_SIZE = 3
CONFIG_SQUASHFS_VMALLOC =
EXTRA_CFLAGS = -I${Sqlzma} -I${SqFs}/../../include -Wall -Werror
EXTRA_CFLAGS += -DCONFIG_SQUASHFS_MODULE -UCONFIG_SQUASHFS
EXTRA_CFLAGS += -UCONFIG_SQUASHFS_EMBEDDED -DCONFIG_SQUASHFS_FRAGMENT_CACHE_SIZE=3
EXTRA_CFLAGS += -UCONFIG_SQUASHFS_VMALLOC
export

all: all_sq
all_sq:
	${MAKE} -C ${KDir} M=${SqFs} C=0 V=0 modules

clean: clean_sq
clean_sq:
	${MAKE} -C ${KDir} M=${SqFs} V=0 clean
endif

########################################

load:
	for i in ${LzmaC}/kmod/unlzma.ko ${LzmaC}/kmod/sqlzma.ko \
		${SqFs}/squashfs.ko; \
	do sudo insmod $$i; done

unload:
	-sudo rmmod squashfs sqlzma unlzma
