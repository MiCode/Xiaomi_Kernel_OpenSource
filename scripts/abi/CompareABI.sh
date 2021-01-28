#!/bin/bash
# SPDX-License-Identifier: GPL-2.0

green='\e[0;32m'
red='\e[0;31m'
eol='\e[0m'

BASE_DIR=$PWD
ABIGAIL_DIR=$BASE_DIR/../kernel/build/abi
ABIGAIL_BUILD_SCRIPT=$ABIGAIL_DIR/bootstrap_src_build
ABI_XML_DIR=$BASE_DIR/scripts/abi/abi_xml
ABI_RESULT_DIR=$BASE_DIR/scripts/abi/abi_xml
ORI_ABI_XML=abi_$src_defconfig.xml
TARGET_ABI_XML=abi_target.xml
ABI_REPORT=abi-report.out
FINAL_ABI_REPORT=abi-report-final.out
TARGET_KERNEL_DIR=$BASE_DIR/out
TARGET_VMLINUX_DIR=$BASE_DIR/out_vmlinux

echo "Get ABIGAIL_VERSION from $ABIGAIL_BUILD_SCRIPT"
ABIGAIL_VERSION=`grep "ABIGAIL_VERSION=" $ABIGAIL_BUILD_SCRIPT | cut -f2- -d=`
ABIGAIL_DIR_RELEASE=$ABIGAIL_DIR/abigail-inst/$ABIGAIL_VERSION
echo "ABIGAIL_DIR_RELEASE=$ABIGAIL_DIR_RELEASE"

function print_usage(){
	echo -e "${green}Script for auto generate target_branch's ABI xml \
based on src_defconfig and compare with abi_{src_defconfig}.xml${eol}"
	echo ""
	echo -e "${red}Command for local test:${eol}"
	echo "[src_defconfig] mode=m ./scripts/abi/CompareABI.sh"
	echo ""
	echo -e "${green}Description:${eol}"
	echo "[src_defconfig]: source project defconfig"
	echo ""
	echo -e "${green}Example:${eol} ${red}src_defconfig=\
k79v1_64_gki_debug_defconfig mode=m \
./scripts/abi/CompareABI.sh 2>&1 | tee buildABI.log${eol}"
	echo ""
	echo -e "${green}Script for auto generate target_branch's ABI xml \
based on src_defconfig and compare with abi_{src_defconfig}.xml and save abi \
monitor result to [abi_result_path]${eol}"
	echo ""
	echo -e "${red}Command for local test:${eol}"
	echo "[src_defconfig] mode=m [abi_result_path] \
./scripts/abi/CompareABI.sh"
	echo ""
	echo -e "${green}Description:${eol}"
	echo "[src_defconfig]: source project defconfig"
	echo ""
	echo "[abi_result_path]: absolute path to put abi monitor result"
	echo ""
	echo -e "${green}Example:${eol} ${red}src_defconfig=\
k79v1_64_gki_debug_defconfig mode=m abi_result_path=absolute_path \
./scripts/abi/CompareABI.sh 2>&1 | tee buildABI.log${eol}"
	echo ""
	echo -e "${red}Command for delete temp files:${eol}"
	echo "mode=d ./scripts/abi/CompareABI.sh"
	echo ""
	echo -e "${green}Description:${eol}"
	echo ""
	echo -e "${green}Example:${eol} ${red}mode=d \
./scripts/abi/CompareABI.sh${eol}"
}
function del_temp_files(){
	echo "Delete temp files $TARGET_KERNEL_DIR"
	rm -rf $TARGET_KERNEL_DIR
	echo "Delete temp files $TARGET_VMLINUX_DIR"
	rm -rf $TARGET_VMLINUX_DIR
	echo "Delete temp files $ABI_RESULT_DIR/$TARGET_ABI_XML"
	rm -rf $ABI_RESULT_DIR/$TARGET_ABI_XML
	echo "Delete temp files $ABI_RESULT_DIR/$ABI_REPORT"
	rm -rf $ABI_RESULT_DIR/$ABI_REPORT
	echo "Delete temp files $ABI_RESULT_DIR/$FINAL_ABI_REPORT"
	rm -rf $ABI_RESULT_DIR/$FINAL_ABI_REPORT
}

if [[ "$1" == "h" ]] || [[ "$1" == "help" ]] || [ -z "mode" ]
then
	print_usage
fi

if [ -z "$abi_result_path" ]
then
	echo "ABI_XML_DIR=$ABI_XML_DIR"
	echo "ABI_RESULT_DIR=$ABI_RESULT_DIR"
else
	ABI_RESULT_DIR=$abi_result_path
	echo "ABI_XML_DIR=$ABI_XML_DIR"
	echo "ABI_RESULT_DIR=$ABI_RESULT_DIR"
fi

if [ "$mode" == "d" ]
then
	del_temp_files
fi

if [ "$mode" == "m" ]
then
	#Check Terminal server support "mosesq" or "dockerq"
	echo "Start to test sever queue..."
	mosesq ls -al
	if [ $? -eq 0 ];
	then
		SERVER_QUEUE=mosesq
	else
		SERVER_QUEUE=dockerq
	fi
	echo "This sever is using queue: $SERVER_QUEUE"

	#Build libabigail first
	$ABIGAIL_BUILD_SCRIPT
	#remove temp files first
	del_temp_files
	echo "Generate .config from src_defconfig:$src_defconfig"
	cd ..
	export PATH=$PWD\
/prebuilts/gcc/linux-x86/aarch64/aarch64-linux-android-4.9/bin/:\
$PWD/prebuilts/clang/host/linux-x86/clang-r377782c/bin/:$PATH
	cd $BASE_DIR
	make ARCH=arm64 CLANG_TRIPLE=aarch64-linux-gnu- \
	CROSS_COMPILE=aarch64-linux-android- CC=clang $src_defconfig O=out
	$SERVER_QUEUE make ARCH=arm64 CLANG_TRIPLE=aarch64-linux-gnu- \
	CROSS_COMPILE=aarch64-linux-android- CC=clang O=out -j24 -k


	echo "Generate ABI xml:$TARGET_ABI_XML from kernel \
tree:$TARGET_VMLINUX_DIR"
	#Use abi_dump to generate $TARGET_ABI_XML
	#export $ABIGAIL_DIR_RELEASE bin and lib
	export PATH=${ABIGAIL_DIR_RELEASE}/bin:${PATH}
	export LD_LIBRARY_PATH=${ABIGAIL_DIR_RELEASE}/lib:\
${ABIGAIL_DIR_RELEASE}/lib/elfutils:${LD_LIBRARY_PATH}
	echo "Copy vmlinux from $TARGET_KERNEL_DIR to $TARGET_VMLINUX_DIR"
	mkdir -p $TARGET_VMLINUX_DIR
	cp $TARGET_KERNEL_DIR/vmlinux $TARGET_VMLINUX_DIR
	cd $ABIGAIL_DIR
	python dump_abi --linux-tree $TARGET_VMLINUX_DIR --out-file \
	$ABI_RESULT_DIR/$TARGET_ABI_XML
	echo "Generate ABI report:$ABI_REPORT from \
--baseline:$ABI_XML_DIR/$ORI_ABI_XML --new:$ABI_RESULT_DIR/$TARGET_ABI_XML"
	python diff_abi --baseline $ABI_XML_DIR/$ORI_ABI_XML \
	--new $ABI_RESULT_DIR/$TARGET_ABI_XML \
	--report $ABI_RESULT_DIR/$ABI_REPORT

	cd $BASE_DIR
	echo "Generate $FINAL_ABI_REPORT"
	abi_result_path=$abi_result_path ./scripts/abi/FinalABI.sh
fi
