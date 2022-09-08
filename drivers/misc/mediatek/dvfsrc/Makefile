ifeq ($(CONFIG_MTK_GCOV_KERNEL),y)
GCOV_PROFILE := y
endif

ccflags-y += \
	-I$(srctree)/drivers/misc/mediatek/include/mt-plat/ \

ifneq ($(strip $(CONFIG_MTK_TINYSYS_SSPM_V0)),)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/sspm/v0/
else ifneq ($(strip $(CONFIG_MTK_TINYSYS_SSPM_V1)),)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/sspm/v1/
else ifneq ($(strip $(CONFIG_MTK_TINYSYS_SSPM_V2)),)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/sspm/v2/
else
ccflags-y += -I$(srctree)/drivers/misc/mediatek/sspm/v3/
endif

obj-$(CONFIG_MTK_DVFSRC_HELPER) += mtk-dvfsrc-helper.o
mtk-dvfsrc-helper-objs += dvfsrc-helper.o dvfsrc-debug-v3.o dvfsrc-sysfs.o dvfsrc-debug-v2.o
mtk-dvfsrc-helper-$(CONFIG_MTK_DVFSRC_MET) += dvfsrc-met.o
mtk-dvfsrc-helper-$(CONFIG_MTK_DVFSRC_MET_MT6873) += dvfsrc-met-v6873.o
mtk-dvfsrc-helper-${CONFIG_MTK_DVFSRC_MET_MT6873} += dvfsrc-met-v6983.o
mtk-dvfsrc-helper-${CONFIG_MTK_DVFSRC_MET_MT6768} += dvfsrc-met-v6768.o dvfsrc-ipi.o
mtk-dvfsrc-helper-${CONFIG_MTK_DVFSRC_MET_MT6765} += dvfsrc-met-v6765.o dvfsrc-ipi.o

obj-$(CONFIG_MTK_DVFSRC_MET_MT6768) += dvfsrc-opp-mt6768.o
obj-$(CONFIG_MTK_DVFSRC_MET_MT6765) += dvfsrc-opp-mt6765.o
