#!/bin/sh
# SPDX-License-Identifier: GPL-2.0

green='\e[0;32m'
red='\e[0;31m'
eol='\e[0m'

BASE_DIR=$PWD
ABI_DIR=$BASE_DIR/scripts/abi
ABI_XML_DIR=$BASE_DIR/scripts/abi/abi_xml
ABI_RESULT_DIR=$BASE_DIR/scripts/abi/abi_xml
ORI_ABI_XML=abi_ori.xml
TARGET_ABI_XML=abi_target.xml
ABI_REPORT=abi-report.out
FINAL_ABI_REPORT=abi-report-final.out

#include abi_white_list to bypass violations
source $ABI_DIR/abi_white_list
#Find Delete/Changed/Added and leaf type change
#check_arr=("\[D\]" "\[C\]" "\[A\]" "^'.*' changed:$")
#Find Delete/Changed and leaf type change
check_arr=("\[D\]" "\[C\]" "^'.*' changed:$")

is_abi_violation_bypass=0
declare -i abi_violation_count=0

function print_usage(){
	echo -e "${green}Script for auto generate \
$ABI_RESULT_DIR/$FINAL_ABI_REPORT from $ABI_RESULT_DIR/$ABI_REPORT ${eol}"
	echo ""
	echo -e "${red}Command for local test:${eol}"
	echo ""
	echo -e "${green}Example:${eol} ${red}./scripts/abi/FinalABI.sh${eol}"
	echo ""
	echo -e "${green}Script for auto generate \
$FINAL_ABI_REPORT by specified abi_result_path ${eol}"
	echo ""
	echo -e "${red}Command for local test:${eol}"
	echo "[abi_result_path] ./scripts/abi/FinalABI.sh"
	echo ""
	echo -e "${green}Description:${eol}"
	echo "[abi_result_path]: absolute path to generate fianl abi report"
	echo ""
	echo -e "${green}Example:${eol} ${red}abi_result_path=absolute_path \
/scripts/abi/FinalABI.sh${eol}"
	exit -1
}

if [ -z "$abi_result_path" ]
then
        echo "ABI_XML_DIR=$ABI_XML_DIR"
        echo "ABI_RESULT_DIR=$ABI_RESULT_DIR"
else
        ABI_RESULT_DIR=$abi_result_path
        echo "ABI_XML_DIR=$ABI_XML_DIR"
        echo "ABI_RESULT_DIR=$ABI_RESULT_DIR"
fi

if [[ "$1" == "h" ]] || [[ "$1" == "help" ]]
then
	print_usage
fi

#remove temp files first
rm -rf $ABI_RESULT_DIR/$FINAL_ABI_REPORT

exec < $ABI_RESULT_DIR/$ABI_REPORT
while read line
do
	for ((i=0; i < ${#check_arr[@]}; i++))
	do
		if [[ $line =~ ${check_arr[$i]} ]]
		then
			is_abi_violation_bypass=0
			for ((j=0; j < ${#bypass_arr[@]}; j++))
			do
				if [[ $line =~ ${bypass_arr[$j]} ]]
				then
					is_abi_violation_bypass=1
					break
				fi
			done

			if [[ $is_abi_violation_bypass == 0 ]]
			then
				abi_violation_count+=1
			fi
			break
		fi
	done
	#write $line to $FINAL_ABI_REPORT if $is_abi_violation = 0
	if [[ $is_abi_violation_bypass == 0 ]]
	then
		echo $line >> $ABI_RESULT_DIR/$FINAL_ABI_REPORT
	fi
done

if [[ $abi_violation_count == 0 ]]
then
	echo "ABI Monitor check PASS!!!"
else
	echo "ABI Monitor check FAILED!!!\
Final abi_violation_count=$abi_violation_count!!!"
echo "Please check report: $ABI_RESULT_DIR/$FINAL_ABI_REPORT for details."
fi
exit $abi_violation_count
