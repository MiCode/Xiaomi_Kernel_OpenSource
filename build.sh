PATH=${PATH}:~/toolchains/google/arm-eabi-4.8/bin/
KERNEL_DIR=$PWD
export USE_CCACHE=1
export CCACHE_DIR=~/android/cache
export ARCH=arm
export SUBARCH=arm
make custom_elixir_defconfig ARCH=arm CROSS_COMPILE=arm-eabi-
make menuconfig
cp .config /home/abhi/Xiaomi_Kernel_OpenSource/arch/arm/configs/custom_elixir1_defconfig
make -j5 ARCH=arm CROSS_COMPILE=arm-eabi-
dtbToolCM -2 -o $KERNEL_DIR/arch/arm/boot/dt.img -s 2048 -p $KERNEL_DIR/scripts/dtc/ $KERNEL_DIR/arch/arm/boot/dts/
