MTK_SWPM_MODULES_FOLDER=$(srctree)/drivers/misc/mediatek/swpm/modules/


MTK_SWPM_DBG_COMMON_NAME=mtk-swpm-dbg-common-v1


ccflags-y += -I$(MTK_SWPM_MODULES_FOLDER)/include/

obj-$(CONFIG_MTK_SWPM_MODULE) += ${MTK_SWPM_DBG_COMMON_NAME}.o
${MTK_SWPM_DBG_COMMON_NAME}-objs += swpm_dbg_fs_common.o
${MTK_SWPM_DBG_COMMON_NAME}-objs += $(SWPM_MODULE_DEBUG_COMMON)
