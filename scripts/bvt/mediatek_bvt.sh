#!/bin/sh
# SPDX-License-Identifier: GPL-2.0

green='\e[0;32m'
red='\e[0;31m'
eol='\e[0m'

function print_usage(){
	echo -e "${green}Script for auto test all config on/off/mod \
to ensure kernel build${eol}"
	echo ""
	echo -e "${red}Command for local test kernel build:${eol}"
	echo "[kernel_dir] [arch] [defconfig] mode=t\
./scripts/bvt/mediatek_bvt.sh"
	echo ""
	echo -e "${green}Description:${eol}"
	echo "[kernel_dir]: kernel directory position"
	echo "[arch]: arm/arm64, position of defconfig"
	echo "[defconfig]: mediatek_debug_defconfig for all config on test"
	echo "             mediatek_config_off_defconfig \
for all config off test"
	echo "             mediatek_module_defconfig for all\
config module test"
	echo ""
	echo -e "${green}Example:${eol} ${red}kernel_dir=\$(pwd) arch=arm64 \
defconfig=mediatek_debug_defconfig mode=t \
./scripts/bvt/mediatek_bvt.sh${eol}"
	echo ""
	echo -e "${red}Command for manual add config list:${eol}"
	echo "[commit] [kernel_dir] mode=a ./scripts/bvt/mediatek_bvt.sh"
	echo ""
	echo -e "${green}Description:${eol}"
	echo "[commit]: last commit ID of config_list"
	echo "[kernel_dir]: kernel directory position"
	echo ""
	echo -e "${green}Example:${eol} ${red}kernel_dir=\$(pwd) \
commit=c1f52584def32d13a7a8c435674e0043e196c323 mode=a \
./scripts/bvt/mediatek_bvt.sh${eol}"
}

if [[ "$1" == "h" ]] || [[ "$1" == "help" ]] || [ -z "mode" ]
then
	print_usage
fi

if [ "$mode" == "a" ]
then
	IFS="+"
	if [ -z "$commit" ]
	then
                new_con=$(git log show -p | grep "+CONFIG_" | grep "=y" \
			| sed '/^$/d' | sed 's/y//g' | sed 's/\+//g' \
			| tr '\n' '+')
	else
		new_con=$(git log "$commit"..HEAD -p | grep "+CONFIG_" \
			| grep "=y" | sed '/^$/d' | sed 's/y//g' \
			| sed 's/\+//g' | tr '\n' '+')
	fi
	for conf in ${new_con[@]}; do
		result=$(grep $conf $kernel_dir/scripts/bvt/config_list )
		if [ -z "$result" ]
		then
			echo ""$conf"y" >> $kernel_dir/scripts/bvt/config_list
		fi
	done
fi

if [ "$mode" == "t" ]
then
	exec < $kernel_dir/scripts/bvt/config_list

	IFS="="
	while read config test
	do
		echo $config $test
		if [ "$defconfig" == "mediatek_debug_defconfig" ]
		then
			sh $kernel_dir/scripts/config --file \
			$kernel_dir/arch/$arch/configs/$defconfig -e $config
		fi

		if [ "$defconfig" == "mediatek_config_off_defconfig" ]
		then
			if [ "$test" == "y" ]
			then
				sh $kernel_dir/scripts/config --file \
				$kernel_dir/arch/$arch/configs/$defconfig \
				-d $config
			fi
		fi

		if [ "$defconfig" == "mediatek_module_defconfig" ]
		then
			if [ "$test" == "y" ]
			then
				sh $kernel_dir/scripts/config --file \
				$kernel_dir/arch/$arch/configs/$defconfig \
				-m $config
			fi
		fi
	done
fi
