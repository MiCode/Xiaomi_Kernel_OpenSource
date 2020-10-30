LPM_MODULES_FOLDER=$(srctree)/drivers/misc/mediatek/lpm/modules/

ccflags-y += -I$(LPM_MODULES_FOLDER)/include/

#Get platform name
MTK_LPM_MODULE_PLATFORM_PLAT_NAME = $(strip $(subst ",, $(CONFIG_MTK_LPM_MODULE_NAME)))

#mtk-lpm platform source files
ifneq ($(MTK_LPM_MODULE_PLATFORM_PLAT_NAME), )
include $(LPM_MODULES_FOLDER)/platform/v1/Makefile
include $(LPM_MODULES_FOLDER)/platform/suspend_v1/Makefile

ccflags-y += -I$(LPM_MODULES_FOLDER)/platform/v1/
ccflags-y += -I$(LPM_MODULES_FOLDER)/platform/suspend_v1/

ccflags-y += ${LPM_MODULE_PLAT_PLATFORM_CFLAGS}
LPM_MODULE_PLAT_PLATFORM = $(LPM_MODULE_PLAT_PLATFORM_OBJS:%.o=v1/%.o)
LPM_MODULE_PLAT_PLATFORM += $(LPM_MODULE_PLAT_PLATFORM_SUSPEND_OBJS:%.o=suspend_v1/%.o)

#Generic build
BUILD_MTK_LPM_PLAT_MODUDLE_NAME=mtk-lpm-plat-${MTK_LPM_MODULE_PLATFORM_PLAT_NAME}
obj-$(CONFIG_MTK_LOW_POWER_MODULE) += ${BUILD_MTK_LPM_PLAT_MODUDLE_NAME}.o

${BUILD_MTK_LPM_PLAT_MODUDLE_NAME}-objs += $(LPM_MODULE_PLAT_PLATFORM)
endif
