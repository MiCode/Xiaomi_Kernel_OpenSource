# SPDX-License-Identifier: GPL-2.0
# Mediatek UFS extended features makefile
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/
ccflags-y += -I$(srctree)/drivers/scsi/ufs/vendor/
ccflags-y += -I$(srctree)/drivers/scsi/ufs/
ccflags-y += -I$(srctree)/drivers/scsi/

obj-$(CONFIG_SCSI_UFS_MEDIATEK) += ufs-mediatek-mod.o
ufs-mediatek-mod-y := ../ufs-mediatek.o
ufs-mediatek-mod-$(CONFIG_UFSFEATURE) += feature/ufsfeature.o feature/ufsfeature-mimic.o
ufs-mediatek-mod-$(CONFIG_UFSSHPB) += feature/hpb/ufsshpb.o
ufs-mediatek-mod-$(CONFIG_UFSTW) += feature/tw/ufstw.o
ufs-mediatek-mod-$(CONFIG_UFSHID) += feature/hid/ufshid.o
ufs-mediatek-mod-$(CONFIG_UFSRINGBUF) += feature/ringbuf/ufsringbuf.o
