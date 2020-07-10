ifeq ($(CONFIG_MTK_GCOV_KERNEL),y)
GCOV_PROFILE := y
endif

ccflags-y += \
	-I$(srctree)/drivers/misc/mediatek/sspm/ \
	-I$(srctree)/drivers/misc/mediatek/include/mt-plat/ \

obj-$(CONFIG_MTK_DVFSRC_HELPER) += mtk-dvfsrc-helper.o
mtk-dvfsrc-helper-objs += dvfsrc-helper.o dvfsrc-debug-v3.o dvfsrc-sysfs.o

obj-${CONFIG_MTK_DVFSRC_MET} += mtk-dvfsrc-met.o
mtk-dvfsrc-met-y = dvfsrc-met.o
mtk-dvfsrc-met-${CONFIG_MTK_DVFSRC_MET_MT6873} += dvfsrc-met-v6873.o
