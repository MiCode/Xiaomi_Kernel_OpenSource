#!/bin/bash
#
# Compile script for kernel
#

SECONDS=0 # builtin bash timer

ZIPNAME="perf-${DEVICE}-miui-$(date '+%Y%m%d-%H%M').zip"

if [ ! -d "toolchain" ]; then
    git clone --depth=1 https://gitlab.com/PixelOS-Devices/playgroundtc -b 17 toolchain
fi

export ARCH=arm64
export KBUILD_BUILD_USER=Builder
export KBUILD_BUILD_HOST=DenomSly
export PATH="$(pwd)/toolchain/bin/:$PATH"


if [[ $1 = "-c" || $1 = "--clean" ]]; then
	rm -rf out
	echo "Cleaned output folder"
fi

make O=out ARCH=arm64 vendor/sweet_user_defconfig
make -j$(nproc) \
    O=out \
    ARCH=arm64 \
    LLVM=1 \
    LLVM_IAS=1 \
    AR=llvm-ar \
    NM=llvm-nm \
    LD=ld.lld \
    OBJCOPY=llvm-objcopy \
    OBJDUMP=llvm-objdump \
    STRIP=llvm-strip \
    CC=clang \
    CROSS_COMPILE=aarch64-linux-gnu- \
    CROSS_COMPILE_ARM32=arm-linux-gnueabi 2>&1 | tee log-compiler.txt

kernel="out/arch/arm64/boot/Image.gz"
dtbo="out/arch/arm64/boot/dtbo.img"
dtb="out/arch/arm64/boot/dtb.img"

if [ ! -f "$kernel" ] || [ ! -f "$dtbo" ] || [ ! -f "$dtb" ]; then
	echo -e "\nCompilation failed!"
	exit 1
fi

echo -e "\nKernel compiled successfully! Zipping up...\n"

if [ -d "$AK3_DIR" ]; then
	cp -r $AK3_DIR AnyKernel3
else
	if ! git clone -q https://github.com/basamaryan/AnyKernel3 -b master AnyKernel3; then
		echo -e "\nAnyKernel3 repo not found locally and couldn't clone from GitHub! Aborting..."
		exit 1
	fi
fi

# Modify anykernel.sh to replace device names
sed -i "s/device\.name1=.*/device.name1=sweet/" AnyKernel3/anykernel.sh
sed -i "s/device\.name2=.*/device.name2=sweetin/" AnyKernel3/anykernel.sh

cp $kernel AnyKernel3
cp $dtbo AnyKernel3
cp $dtb AnyKernel3
cd AnyKernel3
zip -r9 "../$ZIPNAME" * -x .git
cd ..
rm -rf AnyKernel3
echo -e "\nCompleted in $((SECONDS / 60)) minute(s) and $((SECONDS % 60)) second(s) !"
echo "Zip: $ZIPNAME"

if test -z "$(git rev-parse --show-cdup 2>/dev/null)" &&
   head=$(git rev-parse --verify HEAD 2>/dev/null); then
	HASH="$(echo $head | cut -c1-8)"
fi

telegram -f $ZIPNAME -M "Completed in $((SECONDS / 60)) minute(s) and $((SECONDS % 60)) second(s) ! Latest commit: $HASH"
