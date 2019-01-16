
ifneq ($(MTK_ROOT_BUILD),)
$(warning "MTK_ROOT_BUILD secwidevine")
include $(MTK_ROOT_BUILD)/Makefile
else
$(warning "custom build secwidevine")
include $(srctree)/drivers/misc/mediatek/Makefile.custom
endif

ifeq ($(CONFIG_TRUSTONIC_TEE_SUPPORT),y)

$(warning "Build widevine driver")
$(warning "MTK_PATH_PLATFORM=$(MTK_PATH_PLATFORM)")

ccflags-y += -I$(srctree)/drivers/misc/mediatek/mach/$(MTK_PLATFORM)/include/trustzone/utils/tlutils

#Now drm is 32 only
#ifeq ($(CONFIG_ARM64), y)
#ccflags-y += -I$(srctree)/drivers/misc/mediatek/gud/$(MTK_PLATFORM)/gud/MobiCoreKernelApi/include \
#             -I$(srctree)/drivers/misc/mediatek/gud/$(MTK_PLATFORM)/gud/MobiCoreKernelApi/public 
#else
ccflags-y += -I$(srctree)/drivers/misc/mediatek/gud/$(MTK_PLATFORM)/gud/MobiCoreKernelApi/include \
             -I$(srctree)/drivers/misc/mediatek/gud/$(MTK_PLATFORM)/gud/MobiCoreKernelApi/public 
#endif
obj-y += secwidevine.o

endif