#!/bin/bash

#  source the setup script
if [ -z $COMP_PATH_ROOT ]; then
	echo "The build environment is not set!"
	echo "Trying to source setupDrivers.sh automatically!"
	source ../setupDrivers.sh || exit 1
fi

ROOT_PATH=$(dirname $(readlink -f $0))
#  These folders need to be relative to the kernel dir or absolute!
PLATFORM=EXYNOS_5410_STD
CODE_INCLUDE=$(readlink -f $ROOT_PATH/Locals/Code)
PLATFORM_INCLUDE="$CODE_INCLUDE/platforms/$PLATFORM"
MOBICORE_DAEMON=$COMP_PATH_MobiCoreDriverLib/Public

MOBICORE_CFLAGS="-I$MOBICORE_DRIVER/Public -I$MOBICORE_DAEMON -I$COMP_PATH_MobiCore/inc/Mci -I$COMP_PATH_MobiCore/inc -I${PLATFORM_INCLUDE}"

# Clean first
make -C $CODE_INCLUDE clean

make -C $LINUX_PATH \
	MODE=$MODE \
	ARCH=arm \
	CROSS_COMPILE=$CROSS_COMPILE \
	M=$CODE_INCLUDE \
	"MOBICORE_CFLAGS=$MOBICORE_CFLAGS" \
	modules
