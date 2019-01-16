#!/bin/bash
# Default settings
verfile="android.ver"
curcfg=".config"
release="n"
rebuild="n"
clean="n"
makeflags="-w"
makedefs="V=0"
makejobs=${MAKEJOBS}
curdir=`pwd`
if [ "${KBUILD_OUTPUT_SUPPORT}" == "yes" ];then
  outdir=../${MTK_ROOT_OUT}/KERNEL_OBJ
  mkdir -p $outdir
  mkdir -p mediatek/custom
  ln -fns ../../../${MTK_ROOT_CUSTOM_OUT} mediatek/custom/out
fi

usage() {
    echo "Usage: $0 {release|rebuild|clean|silent|verbose|single} [config-xxx]"
    echo "  config file will be generated if build with TARGET_PRODUCT"
    exit 1
}

make_clean() {
    echo "**** Cleaning ****"
    nice make ${makeflags} ${makedefs} distclean
}


# Main starts here
while test -n "$1"; do
    case "$1" in
    release)
        release="y"
    ;;
    rebuild)
        rebuild="y"
    ;;
    clean)
        clean="y"
    ;;
    mrproper)
        mrproper="y"
    ;;
    silent)
        makeflags="-ws"
        makedefs="V=0"
    ;;
    verbose)
        makeflags="-w"
        makedefs="V=1"
    ;;
    single)
        makejobs=""
    ;;
    *)
        export TARGET_PRODUCT=$1
    ;;
    esac
    shift
done

source ../mediatek/build/shell.sh ../ kernel
defcfg="${MTK_ROOT_GEN_CONFIG}/kconfig"
if [ "${KBUILD_OUTPUT_SUPPORT}" == "yes" ]; then
  makeflags+=" O=$outdir"
fi

if [ ! -z $KMOD_PATH ]; then
  if [ ! -e $KMOD_PATH ]; then
     echo "Invalid KMOD_PATH:$KMOD_PATH"
     echo "CURDIR=$curdir"
     exit 1;
  fi
fi

# clean if it is necessary
if [ "${clean}" == "y" ]; then
   if [ ! -z $KMOD_PATH ]; then
      echo "Clean kernel module PROJECT=$MTK_PROJECT PATH=$KMOD_PATH";
      if [ "${KBUILD_OUTPUT_SUPPORT}" == "yes" ]; then
        make M="$KMOD_PATH" O=$outdir clean
      else
        make M="$KMOD_PATH" clean
      fi
      exit $?
   else
      make_clean; exit $?;
   fi
fi

if [ "${mrproper}" == "y" ]; then
  make mrproper; exit $?;
fi

if [ "${rebuild}" == "y" ]; then make_clean; fi

echo "**** Configuring / $defcfg / ****"
# select correct configuration file
if [ "${KBUILD_OUTPUT_SUPPORT}" == "yes" ]; then
  make mediatek-configs O=$outdir
else
  make mediatek-configs
fi

# Config DRAM size according to central Project Configuration file setting
# Todo:
# Need a robust mechanism to control Kconfig content by central Config setting
# Move below segment to a configuration file for extension
if [ "$CUSTOM_DRAM_SIZE" == "3G" ]; then
    # Config DRAM size as 3G (0x18000000).
    sed --in-place=.orig \
        -e 's/\(CONFIG_MAX_DRAM_SIZE_SUPPORT=\).*/\10x18000000/' \
        $outdir/.config
else
  if [ "$CUSTOM_DRAM_SIZE" == "2G" ]; then
      # Config DRAM size as 2G (0x10000000).
      sed --in-place=.orig \
          -e 's/\(CONFIG_MAX_DRAM_SIZE_SUPPORT=\).*/\10x10000000/' \
          -e 's/\(CONFIG_RESERVED_MEM_SIZE_FOR_PMEM=\).*/\10x1700000/' \
          $outdir/.config
  else
    if [ "$CUSTOM_DRAM_SIZE" == "4G" ]; then
        # Config DRAM size as 4G (0x20000000).
        sed --in-place=.orig \
            -e 's/\(CONFIG_MAX_DRAM_SIZE_SUPPORT=\).*/\10x20000000/' \
            $outdir/.config
    else
      if [ "$CUSTOM_DRAM_SIZE" == "6G" ]; then
          # Config DRAM size as 6G (0x30000000).
          sed --in-place=.orig \
              -e 's/\(CONFIG_MAX_DRAM_SIZE_SUPPORT=\).*/\10x30000000/' \
              -e 's/\(CONFIG_CMDLINE=.*\)"/\1 vmalloc=280M"/' \
              -e 's/.*\(CONFIG_HIGHMEM\).*/\1=y/' \
              -e '$ a\# CONFIG_HIGHPTE is not set' \
              -e '$ a\# CONFIG_DEBUG_HIGHMEM is not set' \
              $outdir/.config
      else
        if [ "$CUSTOM_DRAM_SIZE" == "8G" ]; then
            # Config DRAM size as 8G (0x40000000).
            sed --in-place=.orig \
                -e 's/\(CONFIG_MAX_DRAM_SIZE_SUPPORT=\).*/\10x40000000/' \
                -e 's/\(CONFIG_CMDLINE=.*\)"/\1 vmalloc=280M"/' \
                -e 's/.*\(CONFIG_HIGHMEM\).*/\1=y/' \
                -e '$ a\# CONFIG_HIGHPTE is not set' \
                -e '$ a\# CONFIG_DEBUG_HIGHMEM is not set' \
                $outdir/.config
        fi
      fi
    fi
  fi
fi

# update configuration
nice make ${makeflags} ${makedefs} silentoldconfig

if [ ! -z $KMOD_PATH ]; then
  echo "Build kernel module PROJECT=$MTK_PROJECT PATH=$KMOD_PATH";
  if [ "${KBUILD_OUTPUT_SUPPORT}" == "yes" ]; then
    make M="$KMOD_PATH" O=$outdir modules
  else
    make M="$KMOD_PATH" modules
  fi
  exit $?
fi

echo "**** Building ****"
make ${makeflags} ${makejobs} ${makedefs}

if [ $? -ne 0 ]; then exit 1; fi

echo "**** Successfully built kernel ****"

mkimg="${MTK_ROOT_BUILD}/tools/mkimage"
if [ "${KBUILD_OUTPUT_SUPPORT}" == "yes" ]; then
  if [ "${TARGET_ARCH}" == "arm64" ]; then
    kernel_img="${outdir}/arch/arm64/boot/Image"
    if [ "${MTK_APPENDED_DTB_SUPPORT}" == "yes" ]; then
      kernel_zimg="${outdir}/arch/arm64/boot/Image.gz-dtb"
    else
      kernel_zimg="${outdir}/arch/arm64/boot/Image.gz"
  else
    kernel_img="${outdir}/arch/arm/boot/Image"
    if [ "${MTK_APPENDED_DTB_SUPPORT}" == "yes" ]; then
      kernel_zimg="${outdir}/arch/arm/boot/zImage-dtb"
    else
      kernel_zimg="${outdir}/arch/arm/boot/zImage"
  fi
else
  if [ "${TARGET_ARCH}" == "arm64" ]; then
    kernel_img="${curdir}/arch/arm64/boot/Image"
    if [ "${MTK_APPENDED_DTB_SUPPORT}" == "yes" ]; then
      kernel_zimg="${curdir}/arch/arm64/boot/Image.gz-dtb"
    else
      kernel_zimg="${curdir}/arch/arm64/boot/Image.gz"
  else
    kernel_img="${curdir}/arch/arm/boot/Image"
    if [ "${MTK_APPENDED_DTB_SUPPORT}" == "yes" ]; then
      kernel_zimg="${curdir}/arch/arm/boot/zImage-dtb"
    else
      kernel_zimg="${curdir}/arch/arm/boot/zImage"
  fi
fi

echo "**** Generate download images ****"

if [ ! -x ${mkimg} ]; then chmod a+x ${mkimg}; fi

if [ "${KBUILD_OUTPUT_SUPPORT}" == "yes" ]; then
  ${mkimg} ${kernel_zimg} KERNEL > $outdir/kernel_${MTK_PROJECT}.bin
else
  ${mkimg} ${kernel_zimg} KERNEL > kernel_${MTK_PROJECT}.bin
fi

copy_to_legacy_download_flash_folder   $outdir/kernel_${MTK_PROJECT}.bin rootfs_${MTK_PROJECT}.bin
