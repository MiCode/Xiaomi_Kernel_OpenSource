# Copyright (C) 2019 MediaTek Inc.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# See http://www.gnu.org/licenses/gpl-2.0.html for more details.
#
#
demo_mkp =
ifdef demo_mkp
ccflags-y += -DDEMO_MKP
endif

ccflags-y += -I$(src)              # needed for trace events
ccflags-y += -DTEXT_OFFSET=$(TEXT_OFFSET)
ccflags-y += -I$(srctree)/arch/arm/include/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/geniezone/public
ccflags-y += -I$(srctree)/drivers/misc/mediatek/mkp
ccflags-y += -I$(srctree)/security/selinux/include/
obj-$(CONFIG_MTK_MKP) += mkp.o
mkp-$(CONFIG_MTK_MKP) := mkp_main.o
mkp-$(CONFIG_MTK_MKP) += policy.o
mkp-$(CONFIG_MTK_MKP) += mkp_hvc.o
mkp-$(CONFIG_MTK_MKP) += mkp_api.o
mkp-$(CONFIG_MTK_MKP) += ksym.o
ifdef demo_mkp
mkp-$(CONFIG_MTK_MKP) += mkp_rbtree.o
endif
