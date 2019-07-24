ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/lpm/inc/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/lpm/modules/platform/mt6779/

obj-y += mtk_cpuidle_status.o
obj-y += mtk_cpuidle_cpc.o

obj-y += mtk_cpuidle_procfs.o
mtk_cpuidle_procfs-y += mtk_idle_procfs.o mtk_idle_procfs_state.o mtk_idle_procfs_cpc.o mtk_idle_procfs_profile.o  mtk_idle_procfs_control.o
