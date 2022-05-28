
# Add cflags as bellow
#mtk_perf_ioctl_cflags += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/
subdir-ccflags-y += -I$(srctree)/kernel/
mtk_perf_ioctl_objs += perf_ioctl.o
mtk_perf_ioctl_magt_objs += perf_ioctl_magt.o
