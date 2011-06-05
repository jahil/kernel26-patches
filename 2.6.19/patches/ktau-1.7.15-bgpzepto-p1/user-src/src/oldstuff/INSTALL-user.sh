# Temporary ktau_proc_interface install script
# - builds ktau_proc_interface.o
# - create libktau.a
# - build read_ktau, purge_ktau utilities 
# - puts all binaries in ../bin/
# - creates soft link to read_ktau, purge_ktau from /usr/sbin
#
# [Temporary, until configure, makefile have been setup to work with userspace TAU]

#Arguments:
#	$1 - root of kernel source tree to use to for picking ktau-kernel-specific headers

kernelrt=$1

echo Using $kernelrt for kernel-headers

mkdir ../lib

gcc -g -I../include/ -I$1/include -o ../lib/ktau_proc_interface.o -c ktau_proc_interface.c

ar cqs ../lib/libktau.a ../lib/ktau_proc_interface.o

rm ../lib/ktau_proc_interface.o

mkdir ../bin/

gcc -g -I../include/ -I$1/include -L../lib/ -o ../bin/read_ktau read_ktau.c -lktau

gcc -g -I../include/ -I$1/include -L../lib/ -o ../bin/purge_ktau purge_ktau.c -lktau

gcc -g -I../include/ -I$1/include -L../lib/ -o ../bin/toggle_dump_ktau toggle_dump_ktau.c -lktau

ln -f -s `pwd`/../bin/read_ktau /usr/sbin/read_ktau 

ln -f -s `pwd`/../bin/purge_ktau  /usr/sbin/purge_ktau 

ln -f -s `pwd`/../bin/toggle_dump_ktau  /usr/sbin/toggle_dump_ktau 

echo done
