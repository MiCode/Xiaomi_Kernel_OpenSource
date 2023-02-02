
ifeq ($(CONFIG_MTK_FPSGO_V3),$(filter $(CONFIG_MTK_FPSGO_V3),m))
ccflags-y += -DCONFIG_MTK_FPSGO_V3
subdir-ccflags-y += -DCONFIG_MTK_FPSGO_V3
endif

MTK_PM_BUILD_FOLDER=$(srctree)/drivers/misc/mediatek/performance

subdir-ccflags-y += -I$(MTK_PM_BUILD_FOLDER)/perf_ioctl/ \
	-I$(MTK_PM_BUILD_FOLDER)/include/

# mtk_perf_ioctl
include $(MTK_PM_BUILD_FOLDER)/perf_ioctl/build.mk
MTK_PERF_IOCTL = $(mtk_perf_ioctl_objs:%.o=perf_ioctl/%.o)
MTK_PERF_IOCTL_SRC = $(MTK_PERF_IOCTL)
obj-$(CONFIG_MTK_PERFORMANCE_MODULE) += mtk_perf_ioctl.o
mtk_perf_ioctl-objs += $(MTK_PERF_IOCTL_SRC)

# mtk_perf_ioctl_magt
MTK_PERF_IOCTL_MAGT = $(mtk_perf_ioctl_magt_objs:%.o=perf_ioctl/%.o)
MTK_PERF_IOCTL_MAGT_SRC = $(MTK_PERF_IOCTL_MAGT)
obj-$(CONFIG_MTK_PERF_IOCTL_MAGT) += mtk_perf_ioctl_magt.o
mtk_perf_ioctl_magt-objs += $(MTK_PERF_IOCTL_MAGT_SRC)

#Generic subdir build
obj-$(CONFIG_MTK_FPSGO_V3) += fpsgo_v3/
obj-$(CONFIG_MTK_FRS) += frs/

obj-$(CONFIG_MTK_LOAD_TRACKER) += load_track/
obj-$(CONFIG_MTK_LOAD_TRACKER) += uload_ind/
