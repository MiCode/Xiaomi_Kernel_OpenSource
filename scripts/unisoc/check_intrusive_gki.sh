#!/bin/bash

show_usage()
{
        cat <<-EOF

usage: sprd_check_gki [ <option> ]

Options:
  -p, --cktoolpath=PATH  path to the tools directory where the abigail will
                         be built (This must be provided);
  -c, --commitid        the base for aosp commit-id
  -h, --help             show this text and exit.
EOF
}

fail_usage()
{
        [ -z "$1" ] || echo "$1"
        show_usage
        exit 1
}

compile_for_clang()
{
    local global=$1

    make ARCH=arm64 LLVM=1 LLVM=1 DEPMOD=depmod DTC=${DTC_BIN} O=$out gki_defconfig
    ${ROOT_DIR}/${KERNEL_DIR}/scripts/config --file ${out}/.config \
       -e LTO_CLANG \
       -d LTO_NONE \
       -e LTO_CLANG_THIN \
       -d LTO_CLANG_FULL \
       -e THINLTO \
       -d CONFIG_LOCALVERSION_AUTO \
       -d WERROR

    make ARCH=arm64 LLVM=1 LLVM=1 DEPMOD=depmod DTC=${DTC_BIN} O=$out $global -j64
}

copy_specify_dir()
{
    local flag=$1
    local dir=$2

    cd $out
    for ((i=0;i<$flag;i++))
    do
        if [ -f ${PreArrarydir[i]} ]; then
             cp --parents ${PreArrarydir[i]} $dir
        else
            unset PreArrarydir[i]
        fi
    done
}

TEMP=`getopt --options p:,c:,h --longoptions cktoolpath:,commtid:,help -- "$@"` || fail_usage ""
eval set -- "$TEMP"
commitid=
clangpath=
while true; do
        case "$1" in
        -p|--cktoolpath)
        clangpath="$2"
        shift
        ;;
        -c|--commitid)
        commitid="$2"
        shift
        ;;
        -h|--help)
        show_usage
        exit 0
        ;;
        --)
        shift
        break
        ;;
        *) fail_usage "Unrecognized option: $1"
        ;;
        esac
        shift
done

export MAIN_SCRIPT_DIR=$(readlink -f $(dirname $0)/../..)
export ROOT_DIR=$(dirname $MAIN_SCRIPT_DIR)
export KERNEL_DIR=${MAIN_SCRIPT_DIR##*/}

echo "= ROOT_DIR: $ROOT_DIR"
echo "= KERNEL_DIR: $KERNEL_DIR"

export KBUILD_BUILD_TIMESTAMP=$(date "+%Y-%m-%d")
export KBUILD_BUILD_VERSION=1


CLANG_VERSION=`cat ${ROOT_DIR}/${KERNEL_DIR}/build.config.constants | grep "CLANG_VERSION" | awk -F "=" '{print $2}'`
DTC_BIN=${ROOT_DIR}/build/kernel/build-tools/path/linux-x86/dtc
CLANG_BIN=${ROOT_DIR}/prebuilts/clang/host/linux-x86/clang-${CLANG_VERSION}/bin
merge_check=$(git --git-dir=${MAIN_SCRIPT_DIR}/.git log -1 --pretty=short|grep "^Merge:")
set -e
sprd_branch=${sprd_branch:-"korg/sprdlinux5.15"}
aosp_branch=${aosp_branch:-"korg/android13-5.15"}
patch_file=$(git --git-dir=${MAIN_SCRIPT_DIR}/.git log -1 --pretty="format:" --name-only --diff-filter=AMCR)
patch_commit=$(git --git-dir=${MAIN_SCRIPT_DIR}/.git log -1 --pretty="format:%H")
aosp_commit=$(git --git-dir=${MAIN_SCRIPT_DIR}/.git merge-base ${sprd_branch} ${aosp_branch})
echo sprd_branch:$sprd_branch  aosp_branch:$aosp_branch
echo base_commit:$aosp_commit
set +e

# set the common sysroot
sysroot_flags+="--sysroot=${ROOT_DIR}/build/kernel/build-tools/sysroot "

# add openssl (via boringssl) and other prebuilts into the lookup path
cflags+="-I${ROOT_DIR}/prebuilts/kernel-build-tools/linux-x86/include "

# add openssl and further prebuilt libraries into the lookup path
ldflags+="-Wl,-rpath,${ROOT_DIR}/prebuilts/kernel-build-tools/linux-x86/lib64 "
ldflags+="-L ${ROOT_DIR}/prebuilts/kernel-build-tools/linux-x86/lib64 "

# Have host compiler use LLD and compiler-rt.
LLD_COMPILER_RT="-fuse-ld=lld --rtlib=compiler-rt"
ldflags+=${LLD_COMPILER_RT}

export PATH=${CLANG_BIN}:$PATH
export PATH=${ROOT_DIR}/prebuilts/kernel-build-tools/linux-x86/bin:$PATH
export HOSTCFLAGS="$sysroot_flags $cflags"
export HOSTLDFLAGS="$sysroot_flags $ldflags"
temp_def="${MAIN_SCRIPT_DIR}/./tmp_intrusive_check/"
out="$temp_def/out"
patch_prefile_dir="$temp_def/tmp"
aosp_prefile_dir="$temp_def/aosp"
return_val=0
check_cfile_flag=0
check_hfile_flag=0
check_ifile_flag=0

declare -a Arrarydir
declare -a PreArrarydir
declare -a PreArraryfile

if [ -n "${merge_check}" ]; then
    echo "Skip the intrusive check(merge commit) ." >&1
    exit 0
else
set -e
    for file in $patch_file; do
        if [[ "${file##*.}"x == "c"x ]]; then
            cfile=${file/%".c"/".o"}
            Arrarydir[$check_cfile_flag]=$cfile
            let check_cfile_flag+=1
        elif [[ "${file##*.}"x == "h"x  ]]; then
            let check_hfile_flag+=1
            break
        fi
    done

fi

if [ -d $temp_def ]; then
    rm -rf $temp_def
fi

if [ ! -d $patch_prefile_dir ]; then
    mkdir -p $patch_prefile_dir
fi

if [ ! -d $aosp_prefile_dir ]; then
    mkdir -p $aosp_prefile_dir
fi

if [ -n "$clangpath" ]; then
    export PATH=${clangpath}:$PATH
fi

# checkout to aops commit
echo "===============================================" >&1
echo "= check for aosp commit:$aosp_commit" >&1
if [ -n "$commitid" ]; then
    git checkout $commitid
else
    git checkout $aosp_commit
fi

compile_for_clang vmlinux
if [ $check_hfile_flag -ne 0 ]; then
    cd $out
    totalfile=`find . \( -path "./scripts" -o -path "./tools" \) -prune -o -type f  -name "*.o" \
            |grep -v "x86" | grep -v '^\./\.' | grep -v "vmlinux" | grep -v "scripts" | grep -v "tools" \
                |grep -v "usr" | sed 's/\.\///g' | sed 's/\.o/\.c/g'`
    cd $MAIN_SCRIPT_DIR
    for file in $totalfile
    do
        if [ -f $file ]; then
            predir=${file/%".c"/".i"}
            PreArrarydir[$check_ifile_flag]=$predir
            let check_ifile_flag+=1
            global+="$predir "
        fi
    done
    make ARCH=arm64 -i KCFLAGS="-P -U__LINE__ -D__LINE__=0" LLVM=1 DEPMOD=depmod DTC=${DTC_BIN} O=$out $global -j64
    copy_specify_dir $check_ifile_flag $aosp_prefile_dir

elif [ $check_cfile_flag -ne 0 ]; then
    for ((i=0;i<check_cfile_flag;i++))
    do
        if [ ! -f $out/${Arrarydir[i]} ]; then
            unset Arrarydir[i]
        else
            filedir=${Arrarydir[i]}
            predir=${filedir/%".o"/".i"}
            PreArrarydir[$check_ifile_flag]=$predir
            global+="$predir "
            let check_ifile_flag+=1
        fi
    done
    make ARCH=arm64 -i KCFLAGS="-P -U__LINE__ -D__LINE__=0" LLVM=1 DEPMOD=depmod DTC=${DTC_BIN} O=$out $global -j64
    copy_specify_dir $check_ifile_flag $aosp_prefile_dir
fi

# Checkout to patch commit
echo "========================================================" >&1
echo "= check for current patch commit:$patch_commit" >&1
cd $MAIN_SCRIPT_DIR
git checkout $patch_commit
if [ ${#PreArrarydir[@]} -ne 0 ]; then
    compile_for_clang vmlinux
    for predir in ${PreArrarydir[*]}
    do
        global_1+="$predir "
    done
    make ARCH=arm64 KCFLAGS="-P -U__LINE__ -D__LINE__=0" LLVM=1 DEPMOD=depmod DTC=${DTC_BIN} O=$out $global_1 -j64
    cd $out
    for predir in ${PreArrarydir[*]}
    do
        if [ -n "$predir"  ] && [ -f $predir ]; then
            cp --parents $predir $patch_prefile_dir
        fi
    done
else
    echo "========================================================" >&1
    echo "= Modify file list:" >&1
    for file in $patch_file
    do
        echo "   $file" >&1
    done
    echo "The above file is not required for gki intrusive check." >&1
    echo "The gki intrusive check---------------------------pass." >&1
    rm -rf ${temp_def}
    exit 0
fi

set +e

# replace the kernel_version string such as 5.15.41+ ->5.15.41
kernel_version=$( head ${out}/.config -n3 |tail -n1|awk -F" " '{print $3}')
for f in $(find ${temp_def} -name *.i |xargs grep $kernel_version -l)
do
	sed -i "s/${kernel_version}+/$kernel_version/g" $f
done

# Compare file for intrusive check
for ((i=0;i<check_ifile_flag;i++))
do
    if [ -n "${PreArrarydir[i]}" ]; then
	# use the diff to compare twice, the first compare only to print file name
        diff -q --ignore-blank-lines $patch_prefile_dir/${PreArrarydir[i]} \
		$aosp_prefile_dir/${PreArrarydir[i]}
        diff --ignore-blank-lines -y --suppress-common-lines \
		$patch_prefile_dir/${PreArrarydir[i]} \
		$aosp_prefile_dir/${PreArrarydir[i]}
        result_val=$?
        if [ $result_val -eq 0 ] || [[ ${PreArrarydir[i]} == "init/version.i"  ]]; then
            unset PreArrarydir[i]
        fi
    fi
done

echo "========================================================" >&1
echo "= The result of gki intrusive check:" >&1
if [ ${#PreArrarydir[@]} -ne 0  ]; then
    echo "the following file has been intrusively modified:" >&2
    for file in ${PreArrarydir[*]}
    do
        cfile=${file/%".i"/".c"}
        echo "   $cfile"
        let return_val+=1
    done
    echo "Error: gki intrusive check---------------------------fail." >&1
else
    echo "The gki intrusive check---------------------------pass." >&1
fi

rm -rf ${temp_def}
exit $return_val
