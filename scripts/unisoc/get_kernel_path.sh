#!/bin/bash

parent_dir=$(dirname $(readlink -f $0))


if [[ $(basename ${parent_dir}) == "unisoc" ]] && [[ $(basename $)  ]]; then
  echo $(dirname $(dirname ${parent_dir}))
else
  echo $(dirname ${parent_dir})
fi

