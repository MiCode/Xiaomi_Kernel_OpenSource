#!/bin/bash
# Environment Install takes 1h10m, Build takes 
# On E3 1270 v6, 4c8t-3.7Ghz 32G RAM With SSD

cd ~

# Env
sudo apt update && sudo apt -y dist-upgrade
sudo apt install -y gcc g++ python make texinfo texlive bc bison build-essential ccache curl flex g++-multilib gcc-multilib \
    git gnupg gperf imagemagick lib32ncurses5-dev lib32readline-dev lib32z1-dev liblz4-tool libncurses5-dev libsdl1.2-dev \
    libssl-dev libwxgtk3.0-dev libxml2 libxml2-utils lzop pngcrush rsync schedtool squashfs-tools xsltproc zip zlib1g-dev \
    unzip language-pack-zh-hans
# GCC Cross
wget "https://developer.arm.com/-/media/Files/downloads/gnu-a/9.2-2019.12/binrel/gcc-arm-9.2-2019.12-x86_64-aarch64-none-linux-gnu.tar.xz"
xz -d gcc-arm-9.2-2019.12-x86_64-aarch64-none-linux-gnu.tar.xz
tar xf gcc-arm-9.2-2019.12-x86_64-aarch64-none-linux-gnu.tar
mv gcc-arm-9.2-2019.12-x86_64-aarch64-none-linux-gnu .gcc-arm-9.2-2019.12-x86_64-aarch64-none-linux-gnu
# Env
export ARCH=arm64
export SUBARCH=arm64
export PATH=~/.gcc-arm-9.2-2019.12-x86_64-aarch64-none-linux-gnu/bin:$PATH
export CROSS_COMPILE=aarch64-none-linux-gnu-

# CMP
cd ~/bomb-q-oss
make clean 
make mrproper 
args="-j$(nproc --all) O=out ARCH=arm64 SUBARCH=arm64 CROSS_COMPILE=aarch64-none-linux-gnu-" 
make ${args} O=out atom-user_defconfig 
#make CONFIG_DEBUG_SECTION_MISMATCH=y
make  ${args}
