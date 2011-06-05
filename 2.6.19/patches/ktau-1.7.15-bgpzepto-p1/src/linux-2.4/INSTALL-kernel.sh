CWD=`pwd`
if test -d $1 && test -f ../../patches/$2
then
	echo "......... Current Directory is ${CWD}"
	echo "......... Installing KTAU to Linux source in  $1"
	echo "......... Applying patch $2."
	cd $1
	patch -p1 < ${CWD}/../../patches/$2
	cp -r ${CWD}/include/linux/ktau $1/include/linux/
	cp -r ${CWD}/../include/linux/ktau $1/include/linux/
	cp -r ${CWD}/kernel/ktau $1/kernel/
	cp -r ${CWD}/../kernel/ktau $1/kernel/
else
	echo "ERROR: Invalid arguement"
	echo "USAGE: sh INSTALL.sh <linux-src> <patch-name>"
fi
