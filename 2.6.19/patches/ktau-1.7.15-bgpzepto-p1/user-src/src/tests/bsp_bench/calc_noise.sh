#!/bin/sh 
#meant to be used internally - not for disemination...
percent=$1
for i in 32 64 128 256 512 1024 2048; do 
	nonoise=`cat ../../32millisec_strong/N$i/*.output | grep TOTAL | awk '{ print $3 }'`;
	withnoise=`cat N$i/*.output | grep TOTAL | awk '{ print $3 }'`; 
	#echo $withnoise $nonoise | awk '{ print (($1-$2)/$2)*100 }';
	noiseinjected=`echo $nonoise $percent | awk '{ print ($1 * $2)/100 }'`
	totaldil=`echo $withnoise $nonoise | awk '{ print ($1-$2) }'`
	lessnoise=`echo $totaldil $noiseinjected | awk '{ print ($1-$2) }'`
	nmag=`echo $lessnoise $noiseinjected | awk '{ print $1/$2 }'`
	echo N:$i \%noise:$percent mag:$nmag
done
