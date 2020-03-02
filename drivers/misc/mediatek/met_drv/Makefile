ifeq ($(ARCH), arm)
ccflags-y += -DCONFIG_MET_ARM_32BIT
endif	# ifeq ($(ARCH), arm)

obj-y += met_api.o

ifeq ($(PATCHLEVEL),)
$(error VERSION is not defined)
endif

ifeq ($(PATCHLEVEL),)
$(error PATCHLEVEL is not defined)
endif

CONFIG_KERVER_VERSION := $(VERSION).$(PATCHLEVEL)

#CONFIG_MTK_MET_BUILT_IN := y
ifeq ($(CONFIG_MTK_MET_BUILT_IN),y)
ccflags-y += -DMTK_MET_BUILT_IN
MTK_PLATFORM := $(subst ",,$(CONFIG_MTK_PLATFORM))
MET_ROOT_DIR := $(srctree)/../vendor/mediatek/kernel_modules/met_drv/$(CONFIG_KERVER_VERSION)
MET_COMMON_DIR := $(wildcard $(MET_ROOT_DIR)/common)
MET_PLF_DIR := $(wildcard $(MET_ROOT_DIR)/$(MTK_PLATFORM))

ifeq ($(CONFIG_FTRACE),y)
    ifeq ($(CONFIG_TRACING),y)
        FTRACE_READY := y
    endif
endif

$(info ******** Start to built in met_drv for $(MTK_PLATFORM) ********)
ifneq ($(MET_PLF_DIR),)
    ifeq ($(FTRACE_READY),y)
        include $(MET_COMMON_DIR)/Kbuild
    endif
endif

endif