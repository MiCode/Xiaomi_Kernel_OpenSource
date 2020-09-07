
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

# mtk_touch_boost
include $(MTK_PM_BUILD_FOLDER)/tchbst/build.mk
MTK_UTCH = $(mtk_utch_objs:%.o=tchbst/%.o)
MTK_UTCH_SRC = $(MTK_UTCH)
obj-$(CONFIG_MTK_PERFORMANCE_MODULE) += mtk_utch.o
mtk_utch-objs += $(MTK_UTCH_SRC)

#Generic subdir build
obj-$(CONFIG_MTK_FPSGO_V3) += fpsgo_v3/

