ccflags-y += -I$(srctree)/drivers/misc/mediatek/dcm/include/

# Core
obj-$(CONFIG_MTK_DCM_COMMON)	+= mtk_dcm.o

# SoC Drivers
ifneq ($(wildcard $(srctree)/$(src)/mt6873_dcm_internal.c),)
obj-$(CONFIG_MTK_DCM_MT6873) += mt6873_dcm.o
mt6873_dcm-objs := mt6873_dcm_internal.o mt6873_dcm_autogen.o
endif

ifneq ($(wildcard $(srctree)/$(src)/mt6853_dcm_internal.c),)
obj-$(CONFIG_MTK_DCM_MT6853) += mt6853_dcm.o
mt6853_dcm-objs := mt6853_dcm_internal.o mt6853_dcm_autogen.o
endif

ifneq ($(wildcard $(srctree)/$(src)/mt6886_dcm_internal.c),)
obj-$(CONFIG_MTK_DCM_MT6886) += mt6886_dcm.o
mt6886_dcm-objs := mt6886_dcm_internal.o mt6886_dcm_autogen.o
endif

ifneq ($(wildcard $(srctree)/$(src)/mt6983_dcm_internal.c),)
obj-$(CONFIG_MTK_DCM_MT6983) += mt6983_dcm.o
mt6983_dcm-objs := mt6983_dcm_internal.o mt6983_dcm_autogen.o
endif

ifneq ($(wildcard $(srctree)/$(src)/mt6985_dcm_internal.c),)
obj-$(CONFIG_MTK_DCM_MT6985) += mt6985_dcm.o
mt6985_dcm-objs := mt6985_dcm_internal.o mt6985_dcm_autogen.o
endif
