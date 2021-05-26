MTK_LPM_GS_DUMP_VERSION = $(strip $(subst ",, $(CONFIG_MTK_LPM_GS_DUMP_VERSION)))

ccflags-y += -I$(srctree)/drivers/misc/mediatek/lpm/inc/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/lpm/mtk_lpm_fs/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/lpm/modules/include/

obj-y += mtk_dbg_fs_common.o
obj-y += mtk_lpm_timer.o
obj-$(CONFIG_MTK_LPM_GS_DUMP_SUPPORT) += mtk_lpm_pwr_gs.o

obj-$(CONFIG_MTK_LPM_GS_DUMP_SUPPORT) += gs/${MTK_LPM_GS_DUMP_VERSION}/mtk_lpm_pwr_gs_common.o
obj-$(CONFIG_MTK_LPM_GS_DUMP_SUPPORT) += gs/${MTK_LPM_GS_DUMP_VERSION}/mtk_lpm_pwr_gs_dump_clk.o
obj-$(CONFIG_MTK_LPM_GS_DUMP_SUPPORT) += gs/${MTK_LPM_GS_DUMP_VERSION}/mtk_lpm_pwr_gs_dump_pmic.o

