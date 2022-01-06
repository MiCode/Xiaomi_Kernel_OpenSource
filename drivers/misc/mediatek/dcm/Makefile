ccflags-y += -I$(srctree)/include/linux/soc/mediatek/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/dcm/include/

# Core
obj-$(CONFIG_MTK_DCM_COMMON)	+= mtk_dcm.o

# SoC Drivers
obj-$(CONFIG_MTK_DCM_MT6873) += mt6873_dcm.o
mt6873_dcm-objs := mt6873_dcm_internal.o mt6873_dcm_autogen.o
obj-$(CONFIG_MTK_DCM_MT6853) += mt6853_dcm.o
mt6853_dcm-objs := mt6853_dcm_internal.o mt6853_dcm_autogen.o
ifneq ($(wildcard $(srctree)/$(src)/mt6983_dcm_autogen.c),)
obj-$(CONFIG_MTK_DCM_MT6983) += mt6983_dcm.o
mt6983_dcm-objs := mt6983_dcm_internal.o mt6983_dcm_autogen.o
endif
ifneq ($(wildcard $(srctree)/$(src)/mt6879_dcm_autogen.c),)
obj-$(CONFIG_MTK_DCM_MT6879) += mt6879_dcm.o
mt6879_dcm-objs := mt6879_dcm_internal.o mt6879_dcm_autogen.o
endif
ifneq ($(wildcard $(srctree)/$(src)/mt6895_dcm_autogen.c),)
obj-$(CONFIG_MTK_DCM_MT6895) += mt6895_dcm.o
mt6895_dcm-objs := mt6895_dcm_internal.o mt6895_dcm_autogen.o
endif
ifneq ($(wildcard $(srctree)/$(src)/mt6855_dcm_autogen.c),)
obj-$(CONFIG_MTK_DCM_MT6855) += mt6855_dcm.o
mt6855_dcm-objs := mt6855_dcm_internal.o mt6855_dcm_autogen.o
endif
ifneq ($(wildcard $(srctree)/$(src)/mt6833_dcm_autogen.c),)
obj-$(CONFIG_MTK_DCM_MT6833) += mt6833_dcm.o
mt6833_dcm-objs := mt6833_dcm_internal.o mt6833_dcm_autogen.o
endif
ifneq ($(wildcard $(srctree)/$(src)/mt6789_dcm_autogen.c),)
obj-$(CONFIG_MTK_DCM_MT6789) += mt6789_dcm.o
mt6789_dcm-objs := mt6789_dcm_internal.o mt6789_dcm_autogen.o
endif
