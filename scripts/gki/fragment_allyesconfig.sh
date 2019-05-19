#!/bin/bash
# SPDX-License-Identifier: GPL-2.0-only
# Copyright (c) 2019, The Linux Foundation. All rights reserved.

# Script to convert all the =m CONFIGs to =y

usage() {
	echo "Usage: $0 <input_fragment_path> <output_fragment_path>"
	echo "For example $0 arch/arm64/configs/lahaina_GKI.config arch/arm64/configs/lahaina_GKI_allyes.config"
	echo "Note: The output fragment file will be created or overwritten if already exists"
	exit 1
}

if [ "$#" -ne 2 ]; then
	echo "Error: Invalid number of arguments"
	usage
fi

INPUT_FRAG=$1
OUTPUT_FRAG=$2

sed 's/=m$/=y/g' ${INPUT_FRAG} > ${OUTPUT_FRAG}
