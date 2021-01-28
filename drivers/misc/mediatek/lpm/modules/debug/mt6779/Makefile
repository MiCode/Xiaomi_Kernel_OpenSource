ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/lpm/inc/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/lpm/mtk_lpm_fs/

ccflags-y += -I$(srctree)/drivers/misc/mediatek/lpm/modules/include/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/lpm/modules/include/mt6779/

DBG_MODUDLE_NAME=lpm_dbg_mt6779

obj-$(CONFIG_MTK_LPM_MT6779) += ${DBG_MODUDLE_NAME}.o
${DBG_MODUDLE_NAME}-y += mt6779_dbg.o
${DBG_MODUDLE_NAME}-y += mt6779_logger.o
${DBG_MODUDLE_NAME}-y += mt6779_dbg_fs.o
${DBG_MODUDLE_NAME}-y += mt6779_dbg_idle_fs.o
${DBG_MODUDLE_NAME}-y += mt6779_dbg_spm_fs.o

ifeq ($(CONFIG_MTK_LPM_MT6779), m)
${DBG_MODUDLE_NAME}-y += ../common/mtk_dbg_fs_common.o
endif
${DBG_MODUDLE_NAME}-y += mtk_cpupm_dbg.o
${DBG_MODUDLE_NAME}-y += mtk_cpuidle_status.o
${DBG_MODUDLE_NAME}-y += mtk_cpuidle_cpc.o
${DBG_MODUDLE_NAME}-y += mtk_idle_procfs.o
${DBG_MODUDLE_NAME}-y += mtk_idle_procfs_state.o
${DBG_MODUDLE_NAME}-y += mtk_idle_procfs_cpc.o
${DBG_MODUDLE_NAME}-y += mtk_idle_procfs_profile.o
${DBG_MODUDLE_NAME}-y += mtk_idle_procfs_control.o

