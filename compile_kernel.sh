#!/bin/bash

### Prema Chand Alugu (premaca@gmail.com)

### This script is to build 'zImage' and 'dt.img' 
### which will then be used to patch with AnyKernel2 and flash on any ROM

### This is INLINE_KERNEL_COMPILATION

### Create a directory, and keep kernel code, example:
#### premaca@paluguUB:~/KERNEL_COMPILE$ ls
####    arm-eabi-4.8  msm8916
####


## Copy this script inside the kernel directory 
KERNEL_DIR=$PWD
KERNEL_DEFCONFIG=wt88047_kernel_defconfig
KERNEL_TOOLCHAIN=/home/premaca/KERNEL_COMPILE/arm-eabi-4.8/bin/arm-eabi-
DBTTOOLCM_BIN=/bin/
MAKE_JOBS=10
ANY_KERNEL2_DIR=/home/premaca/git-local/AnyKernel2/
FINAL_KERNEL_ZIP=Jerrica-MM-v1.5.zip

export ARCH=arm
export SUBARCH=arm

## Give the path to the toolchain directory that you want kernel to compile with
## Not necessarily to be in the directory where kernel code is present
export CROSS_COMPILE=$KERNEL_TOOLCHAIN

echo "***** Tool chain is set to $KERNEL_TOOLCHAIN *****"
### defconfig file for your device, should be present in 'arch/arm/configs' of
### the kernel code.
echo "***** Kernel defconfig is set to $KERNEL_DEFCONFIG *****"
make $KERNEL_DEFCONFIG

## $1 = clean
##
cleanC=0
if [ $# -ne 0 ]; then
cleanC=1
fi

if [ $cleanC -eq 1 ]
then
echo "***** Going for Clean Compilation *****"
make clean
else
echo "***** Going for Dirty Compilation *****"
fi

## you can tune the job number depends on the cores
make -j$MAKE_JOBS

#exit

## Clone the repo (https://github.com/xiaolu/mkbootimg_tools)
## And copy dtbTool and dtbToolCM into '/bin'
## Give permissions to execute if required
echo "***** Generating DT.IMG *****"
$DBTTOOLCM_BIN/dtbToolCM -2 -o $KERNEL_DIR/arch/arm/boot/dt.img -s 2048 -p $KERNEL_DIR/scripts/dtc/ $KERNEL_DIR/arch/arm/boot/dts/
echo "***** Verify zImage and dt.img *****"
ls $KERNEL_DIR/arch/arm/boot/zImage
ls $KERNEL_DIR/arch/arm/boot/dt.img


## AnyKernel2 Directory existence
## Clone the repo (https://github.com/premaca/AnyKernel2)
### Modify the updater-script, anykernel.sh etc. according to your device
#### Look at the top two commits done for Redmi-2 (wt88047)
echo "***** Check AnyKernel2 Existence *****"
ls $ANY_KERNEL2_DIR
echo "***** Copy the required stuff to AnyKernel2 *****"
rm -f $ANY_KERNEL2_DIR/dtb
rm -f $ANY_KERNEL2_DIR/dt.img
rm -f $ANY_KERNEL2_DIR/zImage
rm -f $ANY_KERNEL2_DIR/$FINAL_KERNEL_ZIP

cp $KERNEL_DIR/arch/arm/boot/zImage $KERNEL_DIR/arch/arm/boot/dt.img $ANY_KERNEL2_DIR/

echo "***** Time for AnyKernel2 *****"
cd $ANY_KERNEL2_DIR/
mv dt.img dtb
zip -r9 $FINAL_KERNEL_ZIP * -x README $FINAL_KERNEL_ZIP
echo "***** Find the Kernel Zip here *****"
ls $ANY_KERNEL2_DIR/$FINAL_KERNEL_ZIP

echo "***** Please Scroll up and verify for any Errors *****"
echo "***** Script exiting Successfully !! *****"

cd $KERNEL_DIR
exit
