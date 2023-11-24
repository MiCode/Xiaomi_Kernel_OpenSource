# SPDX-License-Identifier: GPL-2.0
# Mediatek UFS extended features makefile

ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/
ccflags-y += -I$(srctree)/drivers/scsi/ufs/vendor/
ccflags-y += -I$(srctree)/drivers/scsi/

obj-$(CONFIG_SCSI_UFS_MEDIATEK) += ufs-mediatek-mod.o
ufs-mediatek-mod-y := xiaomi/ufs-mediatek.o
ufs-mediatek-mod-y += xiaomi/ufshcd-pltfrm.o
ufs-mediatek-mod-y += xiaomi/ufshcd.o
ufs-mediatek-mod-y += xiaomi/ufshcd-crypto.o
ufs-mediatek-mod-y += xiaomi/ufs_bsg.o
ufs-mediatek-mod-y += xiaomi/ufs-debugfs.o
ufs-mediatek-mod-y += xiaomi/ufs-sysfs.o
ufs-mediatek-mod-y += xiaomi/mi_ufs_common_ops.o
ufs-mediatek-mod-y += xiaomi/ufs-mediatek-dbg.o
#ufs-mediatek-mod-y += xiaomi/ufshpb.o
ufs-mediatek-mod-$(CONFIG_UFSFEATURE) += feature/ufsfeature.o feature/ufsfeature-mimic.o
ufs-mediatek-mod-$(CONFIG_UFSSHPB) += feature/hpb/ufsshpb.o
ufs-mediatek-mod-$(CONFIG_UFSTW) += feature/tw/ufstw.o
ufs-mediatek-mod-$(CONFIG_UFSHID) += feature/hid/ufshid.o
ufs-mediatek-mod-$(CONFIG_UFSRINGBUF) += feature/ringbuf/ufsringbuf.o
ufs-mediatek-mod-$(CONFIG_SCSI_UFS_XIAOMI_CLD) += feature/cld/mi_cld.o \
							feature/cld/mi_cld_sysfs.o \
							feature/cld/skhynix_cld.o \
							feature/cld/samsung_cld.o \
							feature/cld/kioxia_cld.o \
							feature/cld/micron_cld.o

ufs-mediatek-mod-$(CONFIG_SCSI_UFS_XIAOMI_FFU) += feature/ffu/ufsffu.o