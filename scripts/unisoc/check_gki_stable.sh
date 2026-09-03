#!/bin/bash

show_usage()
{
        cat <<-EOF

usage: sprd_check_gki [ <option> ]

Options:
  -j, --jobs=N           allow to run N jobs simultaneously (default is 24);
  -p, --cktoolpath=PATH  path to the tools directory where the abigail will
                         be built (This must be provided);
  -m, --mode             check GKI for bsp mode
  -l  --lto=STAT         set LTO=[full|thin|none](default is full);
  -h, --help             show this text and exit.
EOF
}

fail_usage()
{
        [ -z "$1" ] || echo "$1"
        show_usage
        exit 1
}

TEMP=`getopt --options j:,p:,l:,m:,h --longoptions jobs:,cktoolpath:,lto:,mode:,help -- "$@"` || fail_usage ""
eval set -- "$TEMP"

jobs=
abipath=
state="full"
mode=""
RET_VAL=0
RET_COUNT=0
check_compile_flag=0
check_abi_flag=0
check_whitelist_flag=0
while true; do
        case "$1" in
        -j|--jobs)
        jobs="$2"
        shift
        ;;
        -p|--cktoolpath)
        abipath="$2"
        shift
        ;;
        -l|--lto)
        state="$2"
        shift
        ;;
        -m|--mode)
        mode="$2"
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

# save change symbols and crc
starttime=`date +'%Y-%m-%d %H:%M:%S'`
start_seconds=$(date --date="$starttime" +%s);
echo "+++++++++++++++++++++check gki start on ${starttime}++++++++++++++++++++"

export MAIN_SCRIPT_DIR=$(readlink -f $(dirname $0)/../..)

export KERNEL_CODE_DIR=$(dirname $MAIN_SCRIPT_DIR)
export KERNEL_DIR=${MAIN_SCRIPT_DIR##*/}

echo "= KERNEL_CODE_DIR: $KERNEL_CODE_DIR"
echo "= KERNEL_DIR: $KERNEL_DIR"
symbols_to_sprd_array=()
symbols_to_sprd_index=0
crc_symbols_array=()
crc_symbols_index=0
symbols_to_google_array=()
symbols_to_google_index=0

cd $KERNEL_CODE_DIR/$KERNEL_DIR
GITTOOL=`find -type d -name ".git"`
GITCHECK=`git log -1`
if [ ! -n "${GITTOOL}" ] || [ ! -n "${GITCHECK}" ]; then
  check_idh_flag=0
  rm -rf .git
  echo "create abigail git repository" >&1
  git init
  git add -A
  git commit -m "abigail git repository"
fi
cd -

cd ${KERNEL_CODE_DIR}
OUT_ABI_DIR=$(find ${KERNEL_CODE_DIR} -maxdepth 1 -type d -name "out_abi")
if [ -d "${OUT_ABI_DIR}" ] && [[ $mode != "bsp" ]]; then
        echo "= Before running gki check, rm ${OUT_ABI_DIR}" >&1
        rm -rf ${OUT_ABI_DIR}
fi


# User build_abi.sh for GKI check
if [[ $mode != "bsp" ]]; then
	LTO=${state} BUILD_CONFIG=${KERNEL_DIR}/build.config.gki.aarch64.ums512 build/build_abi.sh
	if [ $? -ne 0 ];then
		echo "ERROR: build kernel error when exec the build_abi.sh" >&2
	fi
	if [ "$state" != "full" ];then
		echo "WARNING: the abi may not correct when using LTO $status" >&2
	fi
fi

echo "========================================================" >&1
echo "Start to check ABI stable for GKI" >&1

OUT_ABI_DIR=$(find ${KERNEL_CODE_DIR} -maxdepth 1 -type d -name "out_abi")
DIST_DIR=$(find ${OUT_ABI_DIR} -name "dist")
if [ -d "${OUT_ABI_DIR}" ]; then
        ABI_DIR=$(find ${OUT_ABI_DIR} -name "abi.report")
        ABI_DIR_SHORT=$(find ${OUT_ABI_DIR} -name "abi.report.short")
fi

if [ ! -f "${ABI_DIR}" ]; then
	echo "ERROR: abi.report is not exist, some errors may have occurred during build!" >&2
	exit 1
else
	file_size=`ls -l ${ABI_DIR} | awk '{print $5}'`
	file_rows_count=$(awk 'END{print NR}' ${ABI_DIR})
	echo -e "\nabi report info:"
	cat ${ABI_DIR}
	if [ $file_rows_count -gt 5 ]; then
	    let RET_VAL+=4
	    let check_abi_flag+=1
	elif [ ${file_size} -gt 250 ]; then
		let check_abi_flag+=2
	fi
fi

# Start WhiteList Stable Check
echo "========================================================" >&1
echo "Start to check whitelist stable for GKI" >&1

# Copying abi unisoc whitelist to ${DIST_DIR}
if [ -f "${KERNEL_CODE_DIR}/${KERNEL_DIR}/android/abi_gki_aarch64" ]; then
	echo "= copy abi_gki_aarch64 to base symbols" >&1
	cp "${KERNEL_CODE_DIR}/${KERNEL_DIR}/android/abi_gki_aarch64" ${DIST_DIR}/abi_unisoc_base
	cp "${KERNEL_CODE_DIR}/${KERNEL_DIR}/android/abi_gki_aarch64" ${DIST_DIR}/abi_base
fi
if [ -f "${KERNEL_CODE_DIR}/${KERNEL_DIR}/android/abi_gki_aarch64_unisoc" ]; then
	echo "= add unisoc symbols list to abi_unisoc_base" >&1
	echo >> ${DIST_DIR}/abi_unisoc_base
	cat "${KERNEL_CODE_DIR}/${KERNEL_DIR}/android/abi_gki_aarch64_unisoc" >> ${DIST_DIR}/abi_unisoc_base
fi
# Copying abi base symbols to ${DIST_DIR}
echo "= add unisoc symbols list to abi_base" >&1
echo >> ${DIST_DIR}/abi_base
cat ${KERNEL_CODE_DIR}/${KERNEL_DIR}/android/abi_gki_aarch64_* >> ${DIST_DIR}/abi_base

clang_version=`cat ${KERNEL_CODE_DIR}/${KERNEL_DIR}/build.config.constants | grep "CLANG_VERSION" | awk -F "=" '{print $2}'`
clang_path="${KERNEL_CODE_DIR}/prebuilts/clang/host/linux-x86/clang-${clang_version}/bin/"
export PATH="$clang_path:$PATH"
#creat new whitelist
echo "========================================================" >&1
echo " Creating symbols list file" >&1
if [ -d ${clang_path} ]; then
	export PATH="${clang_path}:$PATH"
	whitelist_out_file=abi_symbols_list_new
	${KERNEL_CODE_DIR}/build/abi/extract_symbols ${DIST_DIR}  \
		--whitelist  ${DIST_DIR}/${whitelist_out_file}
	echo "= Comparing symbols list file" >&1
	if [ -f ${DIST_DIR}/diff_whitelist.report ]; then
		rm ${DIST_DIR}/diff_whitelist.report
      		echo "= rm old diff whitelist file!" >&1
	fi
	while read line; do
		if [[ ${line:0:1} == "[" ]] || [[ ${line:0:1} == "#" ]] || [[ $line == "" ]]; then
			continue
		else
			grep "$line" -w ${DIST_DIR}/abi_unisoc_base
			if [ $? -ne 0 ]; then
				grep "$line" -w ${DIST_DIR}/abi_base
				if [ $? -ne 0 ]; then
					# update to google
					symbols_to_google_array[$symbols_to_google_index]=$line
					let symbols_to_google_index++
				else
					# update to sprd
					symbols_to_sprd_array[$symbols_to_sprd_index]=$line
					let symbols_to_sprd_index++
				fi
  			fi
		fi
	done < ${DIST_DIR}/${whitelist_out_file} > /dev/null
	echo -e ""
#	if [ ${#symbols_to_sprd_array[@]} -ne 0 ]; then
#		echo -e "++++ Add the following information to abi_gki_aarch64_unisoc ++++"
#			>${DIST_DIR}/diff_whitelist.report
#		echo -e "++++ Then upstream to google,cherry-pick back after merge ++++"
#			>>${DIST_DIR}/diff_whitelist.report
#		let RET_VAL+=16
#		let check_whitelist_flag+=1
#		for(( i=0;i<${#symbols_to_sprd_array[@]};i++))
#		do
#			echo ${symbols_to_sprd_array[i]} >> ${DIST_DIR}/diff_whitelist.report
#		done
#	fi

	if [ ${#symbols_to_google_array[@]} -ne 0 ]; then
		echo -e "++++  The following information needs to be patched to google  ++++" >>${DIST_DIR}/diff_whitelist.report
		echo -e "++++  Then we take it back next gki-release with whitelist sync  ++++" >>${DIST_DIR}/diff_whitelist.report
		let RET_VAL+=32
		let check_whitelist_flag+=2
		for(( i=0;i<${#symbols_to_google_array[@]};i++))
		do
			echo ${symbols_to_google_array[i]} >> ${DIST_DIR}/diff_whitelist.report
		done
	fi
	if [ ${#symbols_to_sprd_array[@]} -ne 0 ] || [ ${#symbols_to_google_array[@]} -ne 0 ]; then
		cat ${DIST_DIR}/diff_whitelist.report
		echo "++++ list the new symbol belong to ko module name ++++"
		for symb in $(cat ${DIST_DIR}/diff_whitelist.report |grep -v "^+++")
		do
			symb_line=$(grep "$symb" ${DIST_DIR}/${whitelist_out_file} -n|\
				awk -F':' '{print $1}')
			echo "        ${symb}    $(head ${DIST_DIR}/${whitelist_out_file} -n \
				${symb_line}| grep "^# required by" |tail -1)"
		done
	fi
else
	check_whitelist_flag=4
fi

echo "========================================================" >&1
echo "The result of GKI check" >&1

if [ $RET_VAL -ne 0 ]; then
	if [ $check_abi_flag -eq 1 ]; then
		echo -e "= ERROR: ABI Stable check error: KMI has changed!!! abi.report size:${file_size}" >&2
	elif [ $check_abi_flag -eq 2 ]; then
		echo -e "= WARNING: filtered out in ABI check!abi.report size: ${file_size}, rows: ${file_rows_count}" >&2
	fi
	if [ $check_whitelist_flag -eq 4 ]; then
		echo -e "= ERROR: whitelist check error: Clang not find!!!" >&2
	elif [ $check_whitelist_flag -ne 0 ]; then
		echo -e "= ERROR: whitelist check error: whitelist has changed!!!" >&2
	fi
else
	echo -e "= PASS: GKI check--------------------------------------pass! " >&1
fi
echo -e "= More detail for ABI result, please read ${ABI_DIR}"
echo -e "= More detail for whitelist result, please read ${DIST_DIR}/diff_whitelist.report"

# if create a new git repository, remove it
if [[ ${PATCH_TITLE} == "abigail git repository" ]]; then
  echo "remove git repository"
  rm -rf ${KERNEL_CODE_DIR}/${KERNEL_DIR}/.git
fi

endtime=`date +'%Y-%m-%d %H:%M:%S'`
end_seconds=$(date --date="$endtime" +%s);

echo -e  "++++++++++++++++++++++check gki end on $endtime and takes "$((end_seconds-start_seconds))" seconds+++++++++++++++++++++"

exit $RET_VAL
