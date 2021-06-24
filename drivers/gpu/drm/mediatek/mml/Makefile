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

MTK_PLATFORM := mt6893

ccflags-y += -I$(srctree)/drivers/gpu/drm/mediatek 	\
             -I$(srctree)/drivers/devfreq/		\
             -I$(srctree)/drivers/misc/mediatek/mmdvfs/ \
             -I$(srctree)/drivers/misc/mediatek/smi/ \
             -I$(srctree)/drivers/misc/mediatek/smi/$(MTK_PLATFORM)/ \
             -I$(srctree)/drivers/misc/mediatek/mach/$(MTK_PLATFORM)/include \
             -I$(srctree)/drivers/misc/mediatek/include/mt-plat	\
             -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include	\
             -I$(srctree)/drivers/misc/mediatek/m4u/$(MTK_PLATFORM) \
             -I$(srctree)/drivers/iommu/ \
             -I$(srctree)/drivers/misc/mediatek/mtk-interconnect/

obj-$(CONFIG_MTK_MML_MT6893) := mtk-mml-mt6893.o
obj-$(CONFIG_MTK_MML_LEGACY) := mtk-mml-legacy.o

mtk-mml-mt6893-objs := mml_drv.o
mtk-mml-legacy-objs := mml_drv.o
# mml_drv_v1.o if ipbased

mml_drv-objs += mtk-mml-driver.o
mml_drv-objs += mtk-mml-core.o
mml_drv-objs += mtk-mml-drm-adaptor.o
mml_drv-objs += mtk-mml-buf.o
mml_drv-objs += mtk-mml-sys.o
mml_drv-objs += mtk-mml-mutex.o
mml_drv-objs += mtk-mml-rsz.o
mml_drv-objs += mtk-mml-rdma.o
mml_drv-objs += mtk-mml-wrot.o
mml_drv-objs += mtk-mml-tp-mt6893.o
mml_drv-objs += mtk-mml-tile.o
mml_drv-objs += mtk-mml-hdr.o
mml_drv-objs += mtk-mml-color.o
mml_drv-objs += mtk-mml-aal.o
mml_drv-objs += mtk-mml-tdshp.o
mml_drv-objs += mtk-mml-tcc.o
mml_drv-objs += mtk-mml-fg.o

mml_drv-objs += tile_core.o
mml_drv-objs += tile_utility.o
mml_drv-objs += tile_mdp_func.o
mml_drv-objs += DpTileScaler.o
mml_drv-objs += ../mediatek_v2/mtk_sync.o

# for unit test
mml_drv-objs += mtk-mml-test.o
