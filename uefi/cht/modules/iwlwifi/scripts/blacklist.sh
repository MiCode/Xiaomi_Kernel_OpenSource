#!/bin/bash

BLACKLIST_CONF="/etc/modprobe.d/backports.conf"
BLACKLIST_MAP=".blacklist.map"

MODULE_DIR=$1
MODULE_UPDATES=$2

if [[ ! -d $MODULE_DIR ]]; then
	exit
fi

if [[ ! -d $MODULE_UPDATES ]]; then
	exit
fi

mkdir -p $(dirname $BLACKLIST_CONF)
rm -f $BLACKLIST_CONF

echo "# To be used when using backported drivers" > $BLACKLIST_CONF

for i in $(grep -v ^# $BLACKLIST_MAP | awk '{print $2}'); do
	MODULE="${i}.ko"
	MODULE_UPDATE="$(grep -v ^# $BLACKLIST_MAP | grep $i | awk '{print $1}' | head -1).ko"

	COUNT=$(find $MODULE_DIR -type f -name ${MODULE} -or -name ${MODULE}.gz | wc -l)
	COUNT_REPLACE=$(find $MODULE_UPDATES -type f -name ${MODULE_UPDATE} -or -name ${MODULE_UPDATE}.gz | wc -l)

	if [ $COUNT -ne 0 ]; then
		if [ $COUNT_REPLACE -ne 0 ]; then
			echo "Blacklisting $MODULE ..."
			echo blacklist $i >> $BLACKLIST_CONF
		fi
	fi
done
