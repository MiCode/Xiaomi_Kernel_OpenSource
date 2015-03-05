#!/bin/bash

SourceList="atomisp_driver css"
TargetList="css2400b0_v21_build css2401a0_v21_build css2401a0_legacy_v21_build"

function create_link ()
{
	for i in $* ; do
		mkdir -p $TargetDir/$(dirname $i)
		cd $TargetDir/$(dirname $i)
		depth=$(echo $(dirname $i) | awk -F/ '{ print NF }')
		rel_dir="../"
		for j in $(seq $depth) ; do
			rel_dir+="../"
		done
		ln -s ${rel_dir}$i $(basename $i)
	done
}

cd `dirname $0`
BaseDir=$(pwd)
for TargetDir in $TargetList ; do
	echo "Creating links for "$TargetDir
	TargetDir=$BaseDir/$TargetDir
	for SourceDir in $SourceList ; do
		rm -rf $TargetDir/$SourceDir
		cd $BaseDir
		create_link $(find $SourceDir -name *.c -o -name *.h)
	done
done
echo "Done"
