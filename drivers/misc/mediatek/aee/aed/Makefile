#
# Copyright (C) 2015 MediaTek Inc.
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#

subdir-ccflags-y += -Idrivers/misc/mediatek/aee/mrdump/
ifdef CONFIG_MTK_DRAM_LOG_STORE
subdir-ccflags-y += -Idrivers/misc/mediatek/log_store/
endif

ifdef CONFIG_MTK_AEE_AED
obj-y += aed.o
aed-y := aed-main.o aed-debug.o
endif
