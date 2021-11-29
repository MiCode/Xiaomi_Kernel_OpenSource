################################################################################
## Inheriting configs from ACK
. ${ROOT_DIR}/common/build.config.common
. ${ROOT_DIR}/common/build.config.aarch64

################################################################################
## Variant setup
MSM_ARCH=parrot
VARIANTS=(consolidate gki)
[ -z "${VARIANT}" ] && VARIANT=consolidate

if [ -e "${ROOT_DIR}/msm-kernel" -a "${KERNEL_DIR}" = "common" ]; then
	KERNEL_DIR="msm-kernel"
fi

BOOT_IMAGE_HEADER_VERSION=3
BASE_ADDRESS=0x80000000
PAGE_SIZE=4096
BUILD_VENDOR_DLKM=1
SUPER_IMAGE_SIZE=0x10000000
TRIM_UNUSED_MODULES=1

MODULES_LIST_ORDER="1"
[ -z "${DT_OVERLAY_SUPPORT}" ] && DT_OVERLAY_SUPPORT=1

if [ "${KERNEL_CMDLINE_CONSOLE_AUTO}" != "0" ]; then
	KERNEL_VENDOR_CMDLINE+=' console=ttyMSM0,115200n8 earlycon msm_geni_serial.con_enabled=1'
fi

################################################################################
## Inheriting MSM configs
. ${KERNEL_DIR}/build.config.msm.common
. ${KERNEL_DIR}/build.config.msm.gki
