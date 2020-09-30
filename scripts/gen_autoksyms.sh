#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-only

# Create an autoksyms.h header file from the list of all module's needed symbols
# as recorded on the second line of *.mod files and the user-provided symbol
# whitelist.

set -e

output_file="$1"

# Use "make V=1" to debug this script.
case "$KBUILD_VERBOSE" in
*1*)
	set -x
	;;
esac

# We need access to CONFIG_ symbols
. include/config/auto.conf

ksym_wl=/dev/null
if [ -n "$CONFIG_UNUSED_KSYMS_WHITELIST" ]; then
	for UNUSED_KSYMS_WHITELIST_FILE in $CONFIG_UNUSED_KSYMS_WHITELIST; do
		# Use 'eval' to expand the whitelist path and
		# check if it is relative
		eval ksym_wl="$UNUSED_KSYMS_WHITELIST_FILE"
		[ "${ksym_wl}" != "${ksym_wl#/}" ] ||
		ksym_wl="$abs_srctree/$ksym_wl"
		if [ ! -f "$ksym_wl" ] || [ ! -r "$ksym_wl" ]; then
			echo "ERROR: '$ksym_wl' whitelist file not found" >&2
			exit 1
		fi
		ksym_wls="$ksym_wls $ksym_wl"
	done
fi

# Generate a new ksym list file with symbols needed by the current
# set of modules.
cat > "$output_file" << EOT
/*
 * Automatically generated file; DO NOT EDIT.
 */

EOT

[ -f modules.order ] && modlist=modules.order || modlist=/dev/null
sed 's/ko$/mod/' $modlist |
xargs -n1 sed -n -e '2{s/ /\n/g;/^$/!p;}' -- |
cat - $ksym_wls |
sed 's/^#.*//;s/^ *//;/[[abi_symbol_list]]/g' |
sort -u |
sed -e 's/\(.*\)/#define __KSYM_\1 1/' >> "$output_file"

# Special case for modversions (see modpost.c)
if [ -n "$CONFIG_MODVERSIONS" ]; then
	echo "#define __KSYM_module_layout 1" >> "$output_file"
fi

if [ -n "$CONFIG_UNUSED_KSYMS_WHITELIST_ONLY" ] && [ -f "vmlinux" ] ; then
	syms_from_whitelist="$(mktemp)"
	syms_from_vmlinux="$(mktemp)"

	cat $ksym_wls |
	sed 's/^#.*//;s/^ *//;/[[abi_symbol_list]]/g' |
	sort -u > "$syms_from_whitelist"

	$NM --defined-only vmlinux |
	grep "__ksymtab_" |
	sed 's/^.*__ksymtab_//' |
	sort -u > "$syms_from_vmlinux"

	# Forcefully unexport the symbols that are not declared in the whitelist
	syms_to_unexport=$(comm -13 "$syms_from_whitelist" "$syms_from_vmlinux")

	for sym_to_unexport in $syms_to_unexport; do
		sed -i "/^#define __KSYM_${sym_to_unexport} 1/d" \
							"$output_file"
	done

	rm -f "$syms_from_whitelist" "$syms_from_vmlinux"
fi
