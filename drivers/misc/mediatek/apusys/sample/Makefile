#
# Copyright (C) 2019 MediaTek Inc.
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

SAMPLE_VERSION = 1.0
APUSYS_VERSION_POWER = 2.0

ccflags-y += -I$(obj)/../power/$(APUSYS_VERSION_POWER)/
ccflags-y += -I$(obj)/../power/$(APUSYS_VERSION_POWER)/$(MTK_PLATFORM)/
ccflags-y += -I$(obj)/../include/

obj-y += sample.o

sample-objs += $(SAMPLE_VERSION)/sample_drv.o
sample-objs += $(SAMPLE_VERSION)/sample_inf.o

