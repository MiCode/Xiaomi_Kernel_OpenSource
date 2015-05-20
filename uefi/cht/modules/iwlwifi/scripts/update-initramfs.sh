#!/bin/bash
# Copyright 2009-2013        Luis R. Rodriguez <mcgrof@do-not-panic.com>
#
# Since we provide ssb, ethernet modules and most importantly
# DRM drivers, people may want to update the initramfs image
# of their distribution. This can also help people who may
# want to wireless-boot their systems.

KLIB="$1"
ver=$(echo $KLIB | awk -F "/lib/modules/" '{print $2}' | awk -F"/" '{print $1}')
dir=/boot/

LSB_RED_ID=$(/usr/bin/lsb_release -i -s &> /dev/null)

if [[ -z $LSB_RED_ID && -f "/etc/os-release" ]]; then
	# Let's try with os-release. Fedora doesn't have
	# lsb_release anymore.
	LSB_RED_ID=$(sed -n '/^NAME/ s/^NAME=\(.*\)$/\1/p' /etc/os-release)
fi

case $LSB_RED_ID in
"Ubuntu")
	echo "Updating ${LSB_RED_ID}'s initramfs for $ver under $dir ..."
	mkinitramfs -o $dir/initrd.img-$ver $ver
	echo "Will now run update-grub to ensure grub will find the new initramfs ..."
	update-grub
	;;
"Debian")
	echo "Updating ${LSB_RED_ID}'s initramfs for $ver under $dir ..."
	mkinitramfs -o $dir/initrd.img-$ver $ver
	echo "Will now run update-grub to ensure grub will find the new initramfs ..."
	update-grub
	;;
"Fedora")
	# This adds a -compat-drivers suffixed initramfs with a new grub2
	# entry to not override distribution's default stuff.
	INITRAMFS=${dir}initramfs-$ver-compat-drivers.img
	KERNEL=${dir}vmlinuz-$ver
	GRUB_TITLE="Fedora ($ver) with compat-drivers"

	echo "Updating ${LSB_RED_ID}'s initramfs for $ver under $dir ..."
	mkinitrd --force $INITRAMFS $ver

	# If a previous compat-drivers entry for the same kernel exists
	# do not add it again.
	grep -q "${GRUB_TITLE}" /etc/grub2.cfg &> /dev/null
	if [[ "$?" == "1" ]]; then
		echo "Will now run grubby to add a new kernel entry ..."
		# Add a new kernel entry
		grubby --grub2 --copy-default --add-kernel="$KERNEL" --initrd="$INITRAMFS" --title="$GRUB_TITLE"
	fi
	;;
*)
	echo "Note:"
	echo "You may or may not need to update your initramfs, you should if"
	echo "any of the modules installed are part of your initramfs. To add"
	echo "support for your distribution to do this automatically send a"
	echo "patch against \"$(basename $0)\". If your distribution does not"
	echo "require this send a patch with the '/usr/bin/lsb_release -i -s'"
	echo "($LSB_RED_ID) tag for your distribution to avoid this warning."
        ;;
esac
