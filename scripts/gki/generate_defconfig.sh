#!/bin/bash
# SPDX-License-Identifier: GPL-2.0-only
# Copyright (c) 2019, The Linux Foundation. All rights reserved.

# Script to generate a defconfig variant based on the input

usage() {
	echo "Usage: $0 <platform_defconfig_variant>"
	echo "Variants: <platform>-gki_defconfig, <platform>-qgki_defconfig, and <platform>-qgki-debug_defconfig"
	echo "Example: $0 lahaina-gki_defconfig"
	exit 1
}

if [ -z "$1" ]; then
	echo "Error: Failed to pass input argument"
	usage
fi

SCRIPTS_ROOT=$(readlink -f $(dirname $0)/)

PLATFORM_NAME=`echo $1 | sed -r "s/(-gki_defconfig|-qgki_defconfig|-qgki-debug_defconfig)$//"`

PLATFORM_NAME=`echo $PLATFORM_NAME | sed "s/vendor\///g"`

# We should be in the kernel root after the envsetup
source ${SCRIPTS_ROOT}/envsetup.sh $PLATFORM_NAME

KERN_MAKE_ARGS="ARCH=$ARCH \
		CROSS_COMPILE=$CROSS_COMPILE \
		REAL_CC=$REAL_CC \
		CLANG_TRIPLE=$CLANG_TRIPLE \
		HOSTCC=$HOSTCC \
		HOSTLD=$HOSTLD \
		HOSTAR=$HOSTAR \
		"

# Allyes fragment temporarily created on GKI config fragment
QCOM_GKI_ALLYES_FRAG=${CONFIGS_DIR}/${PLATFORM_NAME}_ALLYES_GKI.config

if [ ! -f "${QCOM_GKI_FRAG}" ]; then
	echo "Error: Invalid input"
	usage
fi

REQUIRED_DEFCONFIG=`echo $1 | sed "s/vendor\///g"`

FINAL_DEFCONFIG_BLEND=""

case "$REQUIRED_DEFCONFIG" in
	${PLATFORM_NAME}-qgki-debug_defconfig )
		FINAL_DEFCONFIG_BLEND+=" $QCOM_DEBUG_FRAG"
		;&	# Intentional fallthrough
	${PLATFORM_NAME}-qgki_defconfig )
		FINAL_DEFCONFIG_BLEND+=" $QCOM_QGKI_FRAG"
		${SCRIPTS_ROOT}/fragment_allyesconfig.sh $QCOM_GKI_FRAG $QCOM_GKI_ALLYES_FRAG
		FINAL_DEFCONFIG_BLEND+=" $QCOM_GKI_ALLYES_FRAG "
		;;
	${PLATFORM_NAME}-gki_defconfig )
		FINAL_DEFCONFIG_BLEND+=" $QCOM_GKI_FRAG "
		;&
esac

FINAL_DEFCONFIG_BLEND+=${BASE_DEFCONFIG}

# Reverse the order of the configs for the override to work properly
# Correct order is base_defconfig GKI.config QGKI.config debug.config
FINAL_DEFCONFIG_BLEND=`echo "${FINAL_DEFCONFIG_BLEND}" | awk '{ for (i=NF; i>1; i--) printf("%s ",$i); print $1; }'`

echo "defconfig blend for $REQUIRED_DEFCONFIG: $FINAL_DEFCONFIG_BLEND"

MAKE_ARGS=$KERN_MAKE_ARGS \
	${KERN_SRC}/scripts/kconfig/merge_config.sh $FINAL_DEFCONFIG_BLEND
make $KERN_MAKE_ARGS savedefconfig
mv defconfig $CONFIGS_DIR/$REQUIRED_DEFCONFIG

# Cleanup the allyes config fragment and other generated files
rm -rf $QCOM_GKI_ALLYES_FRAG .config include/config/ include/generated/ arch/$ARCH/include/generated/
