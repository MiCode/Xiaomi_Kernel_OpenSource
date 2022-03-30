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

# Connectivity combo driver
# If KERNELRELEASE is defined, we've been invoked from the
# kernel build system and can use its language.
ifneq ($(KERNELRELEASE),)
    subdir-ccflags-y += -I$(srctree)/
    subdir-ccflags-y += -Werror -I$(srctree)/drivers/misc/mediatek/include/mt-plat

    #ifneq ($(CONFIG_MTK_TINYSYS_SCP_SUPPORT),)
	# SCP
	subdir-ccflags-y += -Werror -I$(srctree)/drivers/misc/mediatek/scp/include
	subdir-ccflags-y += -Werror -I$(srctree)/drivers/misc/mediatek/connectivity/common/
	subdir-ccflags-y += -Werror -I$(srctree)/drivers/misc/mediatek/conn_scp/conap_scp

	# Aoltest
    ifeq ($(CONFIG_MTK_AOL_DEBUG),y)
        subdir-ccflags-y += -Werror -I$(srctree)/drivers/misc/mediatek/conn_scp/aoltest/
    endif

    MODULE_NAME := connscp
	obj-$(CONFIG_MTK_CONN_SCP) += $(MODULE_NAME).o

	# SCP
	$(MODULE_NAME)-objs += conap_scp/msg_thread.o
	#$(MODULE_NAME)-objs += conap_scp/ring_buffer.o
	$(MODULE_NAME)-objs += conap_scp/conap_scp_core.o
	$(MODULE_NAME)-objs += conap_scp/conap_scp_ipi.o
	$(MODULE_NAME)-objs += conap_scp/conap_platform_data.o
	$(MODULE_NAME)-objs += conn_scp.o

	# Aoltest
    ifeq ($(CONFIG_MTK_AOL_DEBUG),y)
		ccflags-y += -D AOLTEST_SUPPORT
		$(MODULE_NAME)-objs += aoltest/aoltest_core.o
		$(MODULE_NAME)-objs += aoltest/aoltest_netlink.o
		$(MODULE_NAME)-objs += aoltest/aoltest_ring_buffer.o
    endif
    #endif

# Otherwise we were called directly from the command line;
# invoke the kernel build system.
else
    KERNELDIR ?= /lib/modules/$(shell uname -r)/build
    PWD  := $(shell pwd)
default:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules
endif
