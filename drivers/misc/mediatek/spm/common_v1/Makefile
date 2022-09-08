ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/$(MTK_PLATFORM)/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/spm/common_v1/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/cpuidle/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/spm/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/spm/mt6768/
ifeq ($(CONFIG_MTK_UFS_SUPPORT), y)
ccflags-y += -I$(srctree)/drivers/scsi/ufs/
endif

# Include cpu dvfs header cpufreq_api.h
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include/mach/
# Include mtk_ram_console.h for aee_xxx api
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/
# Include uart header mtk_uart_api.h
ccflags-y += -I$(srctree)/drivers/misc/mediatek/uart/include/
# Include teei header teei_client_main.h
ifneq ($(filter y m,$(CONFIG_MICROTRUST_TEE_SUPPORT)),)
include $(srctree)/drivers/tee/teei/Makefile.include
endif
# Include pwm header ddp_pwm.h
ccflags-y += -I$(srctree)/drivers/misc/mediatek/video/include/
# Include mcdi header mtk_mcdi_api.h
ccflags-y += -I$(srctree)/drivers/misc/mediatek/mcdi/include/
# Include clkbuf header mtk_clkbuf_ctl.h
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include/clkbuf_v1/
# Include soc header mtk-soc-afe-control.h
ifeq ($(CONFIG_MTK_SND_SOC_NEW_ARCH), y)
ccflags-y += -I$(srctree)/sound/soc/mediatek/$(MTK_PLATFORM)/
ccflags-y += -I$(srctree)/sound/soc/mediatek/common_int/
endif
# Include soc header mtk-sp-afe-external.h
ifeq ($(CONFIG_SND_SOC_MTK_SMART_PHONE), y)
ccflags-y += -I$(srctree)/sound/soc/mediatek/$(MTK_PLATFORM)/
ccflags-y += -I$(srctree)/sound/soc/mediatek/common/
endif
# Include thermal header mtk_thermal.h
ifeq ($(CONFIG_THERMAL), y)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/thermal/$(MTK_PLATFORM)/inc/mach/
endif


#obj-$(CONFIG_MTK_SPM) += mtk_idle_fs/

#obj-$(CONFIG_MTK_SPM) += mtk_idle.o
#obj-$(CONFIG_MTK_SPM) += mtk_idle_select.o
#obj-$(CONFIG_MTK_SPM) += mtk_idle_internal.o
#obj-$(CONFIG_MTK_SPM) += mtk_idle_profile.o
#obj-$(CONFIG_MTK_SPM) += mtk_idle_twam.o
#obj-$(CONFIG_MTK_SPM) += mtk_spm_resource_req.o
#obj-$(CONFIG_MTK_SPM) += mtk_sleep.o
#obj-$(CONFIG_MTK_SPM) += mtk_sleep_init.o
#obj-$(CONFIG_MTK_SPM) += mtk_vcore_dvfs.o
#obj-$(CONFIG_MTK_SPM) += mtk_idle_module.o
#obj-$(CONFIG_MTK_SPM) += mtk_idle_export_legacy.o




