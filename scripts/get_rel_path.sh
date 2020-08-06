# SPDX-License-Identifier: GPL-2.0
# Copyright (c) 2020 Mediatek Inc.

#!/bin/bash

# rel_path <to> <from>
# Generate relative directory path to reach directory <to> from <from>
to=${1}
from=${2}
path=
stem=
prevstem=

if [ ! -n "${to}" ]; then
  exit 1
fi
if [ ! -n "${from}" ]; then
  exit 1
fi
if [ ! -d "${to}" ]; then
  mkdir -p ${to}
fi
to=$(readlink -e "${to}")
from=$(readlink -e "${from}")
if [ ! -n "${to}" ]; then
  exit 1
fi
if [ ! -n "${from}" ]; then
  exit 1
fi

stem=${from}/
while [ "${to#$stem}" == "${to}" -a "${stem}" != "${prevstem}" ]; do
  prevstem=${stem}
  stem=$(readlink -e "${stem}/..")
  if [ "${stem%/}" == "${stem}" ]; then
    stem=${stem}/
  fi
  path=${path}../
done

echo ${path}${to#$stem}
