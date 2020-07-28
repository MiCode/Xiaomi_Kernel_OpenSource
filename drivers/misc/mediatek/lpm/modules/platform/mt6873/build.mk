
ifeq ($(CONFIG_MTK_TINYSYS_SSPM_SUPPORT), y)
MTK_LPM_MODULE_PLAT_PLATFORM_CFLAGS += -I$(srctree)/drivers/misc/mediatek/sspm/v1
endif

ifeq ($(CONFIG_MTK_TINYSYS_MCUPM_SUPPORT),$(filter $(CONFIG_MTK_TINYSYS_MCUPM_SUPPORT),y m))
MTK_LPM_MODULE_PLAT_PLATFORM_CFLAGS += -I$(srctree)/drivers/misc/mediatek/mcupm/
MTK_LPM_MODULE_PLAT_PLATFORM_CFLAGS += -I$(srctree)/drivers/misc/mediatek/mcupm/include/
MTK_LPM_MODULE_PLAT_PLATFORM_CFLAGS += -I$(srctree)/drivers/misc/mediatek/mcupm/$(MTK_PLATFORM)/
endif

MTK_LPM_MODULE_PLAT_PLATFORM_CFLAGS += -I$(srctree)/drivers/misc/mediatek/base/power/include/power_gs_v1

MTK_LPM_MODULE_PLAT_PLATFORM_CFLAGS += -I$(MTK_LPM_MODULES_FOLDER)/include/
MTK_LPM_MODULE_PLAT_PLATFORM_CFLAGS += -I$(MTK_LPM_MODULES_FOLDER)/include/mt6873/

#source files
MTK_LPM_MODULE_PLAT_PLATFORM_OBJS += mt6873.o
MTK_LPM_MODULE_PLAT_PLATFORM_OBJS += suspend/mt6873_suspend.o
MTK_LPM_MODULE_PLAT_PLATFORM_OBJS += mtk_lp_plat_apmcu.o
MTK_LPM_MODULE_PLAT_PLATFORM_OBJS += mtk_lp_plat_apmcu_mbox.o

