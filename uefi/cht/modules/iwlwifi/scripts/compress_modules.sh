#!/bin/bash

set -e

source ./scripts/mod_helpers.sh

if test "$(mod_filename mac80211)" = "mac80211.ko.gz" ; then
	for driver in $(find "$1" -type f -name *.ko); do
		echo COMPRESS $driver
		gzip -9 $driver
	done
fi
