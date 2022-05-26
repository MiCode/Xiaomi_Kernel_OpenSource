# Copyright (c) 2013-2018 TRUSTONIC LIMITED
# All Rights Reserved.
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# version 2 as published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.

#
# Makefile for the Kinibi core and trusted UI drivers
#
export GUD_ROOT_FOLDER := $(dir $(lastword $(MAKEFILE_LIST)))
obj-$(CONFIG_TRUSTONIC_TEE_SUPPORT) := MobiCoreDriver/
obj-$(CONFIG_TRUSTONIC_TRUSTED_UI) += TlcTui/
ifneq (,$(findstring $(CONFIG_MTK_PLATFORM), "mt6853"))
ccflags-y += -DTEE_WORKER_BIG_CORE
endif
