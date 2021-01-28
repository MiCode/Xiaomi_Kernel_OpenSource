# Include mtk_idle_internal.h for MTK_IDLE_OPT_XXX
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/spm/common/
# Include mt-plat/upmu_common.h
# Include mt-plat/mtk_ccci_common.h
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/
# Include cpuidle_v3 headers mtk_cpuidle.h for atf/dormant driver
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include/cpuidle_v3/
# Include dram header "mtk_dramc.h"
ccflags-y += -I$(srctree)/drivers/misc/mediatek/dramc/$(MTK_PLATFORM)/
# Include sspm header "sspm_ipi.h"
ccflags-y += -I$(srctree)/drivers/misc/mediatek/sspm/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/sspm/$(MTK_PLATFORM)/
# Include mtk idle headers and cpuidle driver header for external modules
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include/
# Include mtk_eem.h
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/eem_v2/$(MTK_PLATFORM)/inc/
# Include mmdvfs_mgr.h
ccflags-y += -I$(srctree)/drivers/misc/mediatek/smi/
# Include uart header "mtk_uart_api.h"
ccflags-y += -I$(srctree)/drivers/misc/mediatek/uart/include/
ifeq ($(CONFIG_MTK_DRAMC), y)
# Include dramc header "mtk_dramc.h"
ccflags-y += -I$(srctree)/drivers/misc/mediatek/dramc/$(MTK_PLATFORM)/
endif
# Include sspm header "sspm_ipi.h"
ifeq ($(CONFIG_MTK_TINYSYS_SSPM_SUPPORT), y)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/sspm/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/sspm/$(MTK_PLATFORM)/
endif

obj-y += mtk_spm.o
obj-y += mtk_spm_twam.o
obj-y += mtk_spm_irq.o
ifeq ($(CONFIG_MTK_DRAMC), y)
obj-y += mtk_spm_dram.o
endif
obj-y += mtk_spm_internal.o
obj-y += mtk_spm_suspend.o
obj-y += mtk_spm_suspend_process.o
ifeq ($(CONFIG_MTK_TINYSYS_SSPM_SUPPORT), y)
obj-y += mtk_sspm.o
obj-y += mtk_spm_power.o
endif
obj-y += mtk_spm_fs.o
obj-y += mtk_spm_utils.o
obj-y += mtk_idle_cond_check.o
obj-y += mtk_spm_idle.o
obj-y += mtk_spm_vcore_dvfs.o
obj-y += mtk_idle_module_plat.o
obj-y += mtk_spm_resource_req_console.o
obj-y += models/mtk_sodi.o
obj-y += models/mtk_dpidle.o
