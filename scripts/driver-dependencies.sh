#! /bin/sh
# SPDX-License-Identifier: GPL-2.0
# Copyright (c) 2022, Google LLC. All rights reserved.
# Author: Isaac J. Manjarres <isaacmanjarres@google.com>

DIR="$(dirname $(readlink -f $0))"
DEV_NEEDS_HOST_PATH="${DIR}/dev-needs.sh"

DEV_TMP_DIR="/data/local/tmp"
DEV_NEEDS_DEV_PATH="${DEV_TMP_DIR}/$(basename ${DEV_NEEDS_HOST_PATH})"

REVERSE_DRIVER_DEP_LIST=0
WANT_MOD_LOAD_ORDER=0

TSORTED_DEV_LIST=
OUTPUT_LIST=

prepare_device() {
	adb wait-for-device root >/dev/null
	if [ $? -ne 0 ]; then
		echo "Failed to root device"
		return 1
	fi

	adb push "${DEV_NEEDS_HOST_PATH}" "${DEV_TMP_DIR}/" >/dev/null
	if [ $? -ne 0 ]; then
		echo "Failed to push the dev-needs.sh script to the device"
		return 1
	fi

	return 0
}

cleanup_device() {
	adb shell "rm ${DEV_NEEDS_DEV_PATH}"
}

remove_duplicate_list_entries () {
	local list_entries="${1}"
	local tmp_list_file=$(mktemp)

	echo -n "${list_entries}" > "${tmp_list_file}"

	# Remove duplicates--except for the first occurrence--while keeping
	# the list in the same order
	OUTPUT_LIST=$(cat -n "${tmp_list_file}" | sort -uk2 | sort -n | cut -f2-)
	rm "${tmp_list_file}"
}

get_dev_compat() {
	local dev=$1
	local compat_str=$(adb shell "cat '$dev'/of_node/compatible 2>/dev/null" | tr -d '\0')

	if [ -n "${compat_str}" ]; then
		OUTPUT_LIST+="${compat_str}"$'\n'
	fi
}

devs_to_compat_list() {
	local dev_list="${1}"

	for dev in ${dev_list[@]}; do
		get_dev_compat "${dev}"
	done
}

get_dev_module_name() {
	local dev=$1
	local module_path=$(adb shell "realpath '$dev'/driver/module 2>/dev/null")

	if [ -n "${module_path}" ]; then
		OUTPUT_LIST+=$(basename "${module_path}")$'\n'
	fi
}

devs_to_module_list() {
	local dev_list="${1}"

	for dev in ${dev_list[@]}; do
		get_dev_module_name "${dev}"
	done
}

get_tsorted_dev_list() {
	local tsort_edges

	tsort_edges=$(adb shell ''${DEV_NEEDS_DEV_PATH}' -t\
		$(find /sys/devices -name driver | sed -e "s/\/driver//")')
	TSORTED_DEV_LIST=$(echo "${tsort_edges}" | tsort | sed -e "s/\"//g")
}

while getopts "s:rl" f; do
	case "$f" in
	s) export ANDROID_SERIAL="${OPTARG}" ;;
	r) REVERSE_DRIVER_DEP_LIST=1 ;;
	l) WANT_MOD_LOAD_ORDER=1 ;;
	esac
done

if [ $REVERSE_DRIVER_DEP_LIST = 1 ] && [ $WANT_MOD_LOAD_ORDER = 1 ]; then
	exit 1
elif [ ! -f "${DEV_NEEDS_HOST_PATH}" ]; then
	echo "Kernel does not contain the required dev-needs.sh script"
	exit 1
fi

if ! prepare_device; then
	exit 1
fi

get_tsorted_dev_list

if [ $WANT_MOD_LOAD_ORDER = 1 ]; then
	devs_to_module_list "${TSORTED_DEV_LIST}"
else
	devs_to_compat_list "${TSORTED_DEV_LIST}"
fi

remove_duplicate_list_entries "${OUTPUT_LIST}"

if [ $REVERSE_DRIVER_DEP_LIST = 1 ]; then
	echo "${OUTPUT_LIST}" | tac
else
	echo "${OUTPUT_LIST}"
fi

cleanup_device
