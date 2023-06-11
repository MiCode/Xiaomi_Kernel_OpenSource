#!/bin/bash

# Defined path
MainPath="$(pwd)"
Proton="$(pwd)/../Proton"
Azure="$(pwd)/../Azure"
Any="$(pwd)/../Any"

# Make flashable zip
MakeZip() {
    if [ ! -d $Any ]; then
        git clone https://github.com/Wahid7852/Anykernel.git $Any
        cd $Any
    else
        cd $Any
        git reset --hard
        git checkout master
        git fetch origin master
        git reset --hard origin/master
    fi
    cp -af $MainPath/out/arch/arm64/boot/Image.gz-dtb $Any
    sed -i "s/kernel.string=.*/kernel.string=$KERNEL_NAME by Abdul7852/g" anykernel.sh
    zip -r9 $MainPath/"Stock-Xiaomi-$ZIP_KERNEL_VERSION.zip" * -x .git README.md *placeholder
    cd $MainPath
}

# Clone compiler

Clone_Proton() {
 
    if [ ! -d $Proton ]; then
        git clone --depth=1 https://github.com/kdrag0n/proton-clang -b master $Proton
    else
        cd $Proton
        git fetch origin master
        git checkout FETCH_HEAD
        git branch -D master
        git branch master && git checkout master
        cd $MainPath
    fi
    Proton_Version="$($Proton/bin/clang --version | grep clang)"
}

Clone_Azure() {
 
    if [ ! -d $Azure ]; then
        git clone --depth=1 https://gitlab.com/Panchajanya1999/azure-clang.git -b main $Azure
    else
        cd $Azure
        git fetch origin main
        git checkout FETCH_HEAD
        git branch -D main
        git branch main && git checkout main
        cd $MainPath
    fi
    Azure_Version="$($Azure/bin/clang --version | grep clang)"
}

# Defined config
HeadCommit="$(git log --pretty=format:'%h' -1)"
export ARCH="arm64"
export SUBARCH="arm64"
export KBUILD_BUILD_USER="Abdul7852"
export KBUILD_BUILD_HOST="-Stable"
Defconfig="begonia_user_defconfig"
KERNEL_NAME=$(cat "$MainPath/arch/arm64/configs/$Defconfig" | grep "CONFIG_LOCALVERSION=" | sed 's/CONFIG_LOCALVERSION="-*//g' | sed 's/"*//g' )
ZIP_KERNEL_VERSION="4.14.$(cat "$MainPath/Makefile" | grep "SUBLEVEL =" | sed 's/SUBLEVEL = *//g')"

# Start building

Build_Proton() {
    Compiler=Proton

    rm -rf out
    TIME=$(date +"%m%d%H%M")
    BUILD_START=$(date +"%s")

    make  -j$(nproc --all)  O=out ARCH=arm64 SUBARCH=arm64 $Defconfig
    exec 2> >(tee -a out/error.log >&2)
    make  -j$(nproc --all)  O=out \
                            PATH="$Proton/bin:/usr/bin:$PATH" \
                            CC=clang \
                            AS=llvm-as \
                            NM=llvm-nm \
                            OBJCOPY=llvm-objcopy \
                            OBJDUMP=llvm-objdump \
                            STRIP=llvm-strip \
                            LD=ld.lld \
                            CROSS_COMPILE=aarch64-linux-gnu- \
                            CROSS_COMPILE_ARM32=arm-linux-gnueabi-
}

Build_Azure() {
    Compiler=Azure

    rm -rf out
    TIME=$(date +"%m%d%H%M")
    BUILD_START=$(date +"%s")

    make  -j$(nproc --all)  O=out ARCH=arm64 SUBARCH=arm64 $Defconfig
    exec 2> >(tee -a out/error.log >&2)
    make  -j$(nproc --all)  O=out \
                            PATH="$Azure/bin:/usr/bin:$PATH" \
                            CC=clang \
			    LLVM=1 \
			    LLVM_IAS=1 \
			    AR=llvm-ar\
                            NM=llvm-nm \
                            OBJCOPY=llvm-objcopy \
                            OBJDUMP=llvm-objdump \
                            STRIP=llvm-strip \
                            LD=ld.lld \
			    CROSS_COMPILE=aarch64-linux-gnu- \
                            CROSS_COMPILE_ARM32=arm-linux-gnueabi-
}

# End with success or fail
End() {
    if [ -e $MainPath/out/arch/arm64/boot/Image.gz-dtb ]; then
        BUILD_END=$(date +"%s")
        DIFF=$((BUILD_END - BUILD_START))
        MakeZip
        ZIP=$(echo *$Compiler*$TIME*.zip)

        echo "Build success in : $((DIFF / 60)) minute(s) and $((DIFF % 60)) second(s)"
       
    else
        BUILD_END=$(date +"%s")
        DIFF=$((BUILD_END - BUILD_START))

        echo "Build fail in : $((DIFF / 60)) minute(s) and $((DIFF % 60)) second(s)"

    fi
}

Text="Start to build kernel"

# Build choices

Proton() {
 
    Clone_Proton
    Build_Proton
    End
}

Azure() {
 
    Clone_Azure
    Build_Azure
    End
}
