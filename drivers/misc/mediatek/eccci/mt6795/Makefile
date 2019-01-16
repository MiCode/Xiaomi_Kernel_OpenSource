ifeq ($(CONFIG_MTK_ECCCI_DRIVER), y)

include $(srctree)/drivers/misc/mediatek/Makefile.custom
ccflags-y += -I$(srctree)/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/eccci/$(MTK_PLATFORM)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/eccci/
# security objects
MASP_CORE_DRIVER_DIR := $(srctree)/drivers/misc/mediatek/drivers/masp

ifeq ($(CONFIG_MTK_SECURITY_SW_SUPPORT), y)
ifeq ($(CONFIG_CUSTOM_SEC_AUTH_SUPPORT), y)
clean-files += $(CCCI_CUSTOM_DRIVER_DIR)/cust_auth.o
else
clean-files += $(patsubst $(obj)/%,%,$(wildcard auth/*.o))
endif
endif

ifeq ($(CONFIG_MTK_SECURITY_SW_SUPPORT), y)
ifeq ($(CONFIG_CUSTOM_SEC_AUTH_SUPPORT), y)
CCCI_CUSTOM_DRIVER_DIR := $(MTK_PATH_CUSTOM)/$(call lc,$(MTK_PROJECT))/kernel/ccci
ccci_plat-y += $(CCCI_CUSTOM_DRIVER_DIR)/cust_auth.o
endif
endif

ifeq ($(CONFIG_MTK_SECURITY_SW_SUPPORT), y)
ccflags-y  += -DENABLE_MD_IMG_SECURITY_FEATURE
endif
ifeq ($(CONFIG_MTK_SEC_MODEM_NVRAM_ANTI_CLONE), y)
ccflags-y  += -DMTK_SEC_MODEM_NVRAM_ANTI_CLONE
endif

# CCCI objects
obj-$(CONFIG_MTK_ECCCI_DRIVER) := ccci.o

# platform dependent parts
ccci-y := ccci_platform.o $(core_obj) $(mach_obj) $(sec_obj)
ifeq ($(CONFIG_MTK_ECCCI_CLDMA), y)
ccci-y += cldma_platform.o
endif

else
obj-  := dummy.o # avoid build error
endif
