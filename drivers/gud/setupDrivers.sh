#!/bin/bash
export COMP_PATH_ROOT=$(dirname $(readlink -f $BASH_SOURCE)) #set this to the absolute path of the folder containing this file

# This part has to be set by the customer
# To be set, absolute path of kernel folder
export LINUX_PATH=
# To be set, absolute path! CROSS_COMPILE variable needed by kernel eg /home/user/arm-2009q3/bin/arm-none-linux-gnueabi-
export CROSS_COMPILE=
# To be set, build mode debug or release
export MODE=debug
# To be set, the absolute path to the Linux Android NDK
export NDK_PATH=

# Global variables needed by build scripts
export COMP_PATH_Logwrapper=$COMP_PATH_ROOT/Logwrapper/Out
export COMP_PATH_MobiCore=$COMP_PATH_ROOT/MobiCore/Out
export COMP_PATH_MobiCoreDriverMod=$COMP_PATH_ROOT/mobicore_driver/Out
export COMP_PATH_MobiCoreDriverLib=$COMP_PATH_ROOT/daemon/Out
export COMP_PATH_AndroidNdkLinux=$NDK_PATH
