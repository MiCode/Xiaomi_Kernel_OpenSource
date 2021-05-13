
SWPM_MODULES_FOLDER=$(srctree)/drivers/misc/mediatek/swpm/modules/

MTK_SWPM_DBG_VERSION = v1

ccflags-y += -I$(srctree)/drivers/misc/mediatek/swpm/modules/include/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/swpm/modules/debug/$(MTK_SWPM_DBG_VERSION)/
ccflags-y += -I$(SWPM_MODULES_FOLDER)/debug/v6893/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/
ccflags-y += -I$(SWPM_MODULES_FOLDER)/include/v6893/

BUILD_SWPM_DBG_MODUDLE_NAME=mtk-swpm-dbg-v6893
obj-$(CONFIG_MTK_SWPM_MODULE) += ${BUILD_SWPM_DBG_MODUDLE_NAME}.o

${BUILD_SWPM_DBG_MODUDLE_NAME}-objs += swpm_v6893_dbg_init.o
