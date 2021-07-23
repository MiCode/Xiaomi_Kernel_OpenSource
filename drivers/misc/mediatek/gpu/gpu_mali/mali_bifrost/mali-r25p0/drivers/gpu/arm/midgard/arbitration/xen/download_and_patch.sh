#!/bin/bash

# Copyright 2020 Arm Limited.
# SPDX-License-Identifier: Apache-2.0
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# Usage:
#   cd <your_path/xen>
#   ./download_and_patch.sh

set -e

PATCH_LIST=("host-xen.patch")
PATCHES=$PWD/patches

mkdir -p host

function prep_host_xen() {
    echo "Preparing Xen hypervisor..."
    pushd host
    if [ ! -d "xen/.git" ]; then
        git clone git://xenbits.xen.org/xen.git
    fi
    pushd xen/xen
    git stash
    git clean -fd
    git checkout RELEASE-4.11.0
    git revert --no-edit 9f954a5e90414d10632e6c2fef5a33ea8a4a1e4e
    popd
    popd
}

function apply_patches() {
    echo "Applying patches"
    for f in "${PATCH_LIST[@]}" ; do
        echo "Patch file $f"
        patch -f -N -p 1 -i $PATCHES/$f
    done
}

prep_host_xen
if [ -d "$PATCHES" ]; then
    apply_patches
else
    echo "Error: missing patches folder!"
    echo "Please copy the patches folder in the current directory!"
    exit -1
fi
echo "Script complete"
