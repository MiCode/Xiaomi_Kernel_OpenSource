#!/bin/bash

if [ -z $COMP_PATH_ROOT ]; then
	echo "The build environment is not set!"
	echo "Trying to source setupDrivers.sh automatically!"
	source ../setupDrivers.sh || exit 1
fi

ROOT_PATH=$(dirname $(readlink -f $BASH_SOURCE))
# These folders need to be relative to the kernel dir or absolute!
PLATFORM=EXYNOS_4X12_STD
CODE_INCLUDE=$(readlink -f $ROOT_PATH/Locals/Code)

MOBICORE_DRIVER=$COMP_PATH_MobiCoreDriverMod
MOBICORE_DAEMON=$COMP_PATH_MobiCoreDriverLib/Public
MOBICORE_CFLAGS="-I$MOBICORE_DRIVER/Public -I$MOBICORE_DAEMON -I$COMP_PATH_MobiCore/inc/Mci -I$COMP_PATH_MobiCore/inc -I$CODE_INCLUDE/include -I$CODE_INCLUDE/public"
MCDRV_SYMBOLS_FILE="$COMP_PATH_ROOT/MobiCoreDriverMod/Locals/Code/Module.symvers"

if [ ! -f $MCDRV_SYMBOLS_FILE ]; then
	echo "Please build the Mobicore Driver Module first!"
	echo "Otherwise you will see warnings of missing symbols"
fi

# Clean first
make -C $CODE_INCLUDE clean

make -C $LINUX_PATH \
	MODE=$MODE \
	ARCH=arm \
	CROSS_COMPILE=$CROSS_COMPILE \
	M=$CODE_INCLUDE \
	"MOBICORE_CFLAGS=$MOBICORE_CFLAGS" \
	MCDRV_SYMBOLS_FILE=$MCDRV_SYMBOLS_FILE \
	modules
