#!/bin/bash
# SPDX-License-Identifier: GPL-2.0

# disable debugfs for user builds
export MAKE_ARGS=$@

if [[ ${SKIP_ENABLE_DEBUG_FS} == "false" ]]; then
    if [[ ${ENABLE_DEBUG_FS} == "true" ]]; then
        echo "Enable debugfs baseon ENABLE_DEBUG_FS:'$ENABLE_DEBUG_FS'"
        echo "Overriding kernel config with CONFIG_DEBUG_FS=y to '$DEFCONFIG'"; \
	    echo -e "\n#KERNEL_CONFIG_OVERRIDE_DEBUG_FS\n" >> ${KERNEL_DIR}/arch/${ARCH}/configs/$DEFCONFIG; \
	    echo "CONFIG_DEBUG_FS=y" >> ${KERNEL_DIR}/arch/${ARCH}/configs/$DEFCONFIG; \
	    echo -e "\n" >> ${KERNEL_DIR}/arch/${ARCH}/configs/$DEFCONFIG;
    else
        echo "disable debugfs baseon DISABLE_DEBUGFS:'$ENABLE_DEBUG_FS'"
        sed -i '/CONFIG_DEBUG_FS=y/d' ${KERNEL_DIR}/arch/${ARCH}/configs/$DEFCONFIG
    fi
fi

echo "disable debugfs baseon DISABLE_DEBUGFS:'$DISABLE_DEBUGFS'"
if [[ ${DISABLE_DEBUGFS} == "true" ]]; then
	echo "build variant ${TARGET_BUILD_VARIANT}"
	if [ ${TARGET_BUILD_VARIANT} == "user" ] && \
		[ ${ARCH} == "arm64" ]; then
		echo "combining fragments for user build"
		(cd $KERNEL_DIR && \
		ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE}\
		./scripts/kconfig/merge_config.sh \
		./arch/${ARCH}/configs/$DEFCONFIG \
		./arch/${ARCH}/configs/vendor/debugfs.config
		make ${MAKE_ARGS} ARCH=${ARCH} \
		CROSS_COMPILE=${CROSS_COMPILE} savedefconfig
		mv defconfig ./arch/${ARCH}/configs/$DEFCONFIG
		rm .config)
	else
		if [[ ${DEFCONFIG} == *"perf_defconfig" ]] && \
			[ ${ARCH} == "arm64" ]; then
			echo "resetting perf defconfig"
			(cd ${KERNEL_DIR} && \
			git checkout arch/$ARCH/configs/$DEFCONFIG)
		fi
	fi
fi
