# SPDX-License-Identifier: GPL-2.0-only
TECHPACK?=y

techpack-dirs := $(shell find $(srctree)/techpack -maxdepth 1 -mindepth 1 -type d -not -name ".*")
obj-${TECHPACK} += stub/ $(addsuffix /,$(subst $(srctree)/techpack/,,$(techpack-dirs)))

techpack-header-dirs := $(shell find $(srctree)/techpack -maxdepth 1 -mindepth 1 -type d -not -name ".*")
header-${TECHPACK} += $(addsuffix /include/uapi/,$(subst $(srctree)/techpack/,,$(techpack-header-dirs)))
