
# Add cflags as bellow
#mtk_lpm_mtk_lpm_fs_cflags += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/

mtk_lpm_mtk_lpm_fs_objs += mtk_cpuidle_sysfs.o
mtk_lpm_mtk_lpm_fs_objs += mtk_lp_sysfs.o
mtk_lpm_mtk_lpm_fs_objs += mtk_suspend_sysfs.o
mtk_lpm_mtk_lpm_fs_objs += mtk_spm_sysfs.o
mtk_lpm_mtk_lpm_fs_objs += mtk_lpm_sysfs.o
mtk_lpm_mtk_lpm_fs_objs += mtk_lp_sysfs_procfs.o
