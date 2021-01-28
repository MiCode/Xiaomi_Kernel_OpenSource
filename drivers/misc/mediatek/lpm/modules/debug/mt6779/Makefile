ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/lpm/inc/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/lpm/mtk_lpm_fs/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/lpm/modules/include/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/lpm/modules/include/mt6779/

BUILD_MTK_LPM_DBG_MODUDLE_NAME=mtk-lpm-dbg-$(strip $(subst ",, $(CONFIG_MTK_LPM_MODULE_NAME)))

obj-$(CONFIG_MTK_LPM_MT6779) += ${BUILD_MTK_LPM_DBG_MODUDLE_NAME}.o


ifeq ($(CONFIG_MTK_LPM_DBG_COMMON),$(filter $(CONFIG_MTK_LPM_DBG_COMMON),m y))
ccflags-y += -DMTK_LPM_DBG_COMMON
${BUILD_MTK_LPM_DBG_MODUDLE_NAME}-y += ../common/mtk_dbg_fs_common.o
${BUILD_MTK_LPM_DBG_MODUDLE_NAME}-y += ../common/mtk_lpm_timer.o
endif

${BUILD_MTK_LPM_DBG_MODUDLE_NAME}-y += mt6779_dbg.o
${BUILD_MTK_LPM_DBG_MODUDLE_NAME}-y += mt6779_logger.o
${BUILD_MTK_LPM_DBG_MODUDLE_NAME}-y += mt6779_dbg_fs.o
${BUILD_MTK_LPM_DBG_MODUDLE_NAME}-y += mt6779_dbg_idle_fs.o
${BUILD_MTK_LPM_DBG_MODUDLE_NAME}-y += mt6779_dbg_spm_fs.o

${BUILD_MTK_LPM_DBG_MODUDLE_NAME}-y += mtk_cpupm_dbg.o
${BUILD_MTK_LPM_DBG_MODUDLE_NAME}-y += mtk_cpuidle_status.o
${BUILD_MTK_LPM_DBG_MODUDLE_NAME}-y += mtk_cpuidle_cpc.o
${BUILD_MTK_LPM_DBG_MODUDLE_NAME}-y += mtk_idle_procfs.o
${BUILD_MTK_LPM_DBG_MODUDLE_NAME}-y += mtk_idle_procfs_state.o
${BUILD_MTK_LPM_DBG_MODUDLE_NAME}-y += mtk_idle_procfs_cpc.o
${BUILD_MTK_LPM_DBG_MODUDLE_NAME}-y += mtk_idle_procfs_profile.o
${BUILD_MTK_LPM_DBG_MODUDLE_NAME}-y += mtk_idle_procfs_control.o

