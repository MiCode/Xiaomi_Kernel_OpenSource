#
# Copyright (C) 2021 MediaTek Inc.
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
# $(error Bad MML)



ccflags-y += -I$(srctree)/drivers/gpu/drm/mediatek 		\
             -I$(srctree)/drivers/devfreq/				\
             -I$(srctree)/drivers/misc/mediatek/mmdvfs/

obj-$(CONFIG_MTK_MML_MT6893) := mtk-mml-mt6893.o

mtk-mml-mt6893-objs := mml_drv.o
# mml_drv_v1.o if ipbased 

mml_drv-objs += mtk-mml-driver.o
mml_drv-objs += mtk-mml-core.o
mml_drv-objs += mtk-mml-drm-adaptor.o
mml_drv-objs += mtk-mml-rsz.o
mml_drv-objs += mtk-mml-rdma.o
mml_drv-objs += mtk-mml-wrot.o
