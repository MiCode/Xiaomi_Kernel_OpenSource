#
# Makefile for mtee kree components
#

#subdir-ccflags-y += -Werror
$(info ************  drivers/trusty/mtee-kree mk ************)

ccflags-y := -I$(srctree)/drivers/misc/mediatek/geniezone/public -Werror $(ccflags-y)

ifneq ($(CONFIG_MACH_MT6758), y)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/ppm_v3/src/mach/$(CONFIG_MTK_PLATFORM)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include/ppm_v3
endif

ifeq ($(CONFIG_MTK_TEE_GP_SUPPORT),y)
  ccflags-y += -I$(srctree)/arch/arm/mach-$(MTK_PLATFORM)/include/trustzone/utils/tlutils
  ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include/trustzone/utils/tlutils
  ccflags-y += -I$(src)/$(MTK_PLATFORM)
  ifeq ($(CONFIG_TRUSTONIC_TEE_SUPPORT),y)
    include $(srctree)/drivers/misc/mediatek/gud/Makefile.include
  else ifeq ($(CONFIG_MICROTRUST_TEE_SUPPORT),y)
    include $(srctree)/drivers/misc/mediatek/teei/Makefile.include
  else ifeq ($(CONFIG_BLOWFISH_TEE_SUPPORT),y)
    ccflags-y += -I$(srctree)/drivers/misc/tzdev
  else
  endif
else # Legacy
  ifeq ($(CONFIG_TRUSTONIC_TEE_SUPPORT),y)
    ccflags-y += -I$(srctree)/arch/arm/mach-$(MTK_PLATFORM)/include/trustzone/utils/tlutils
    ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include/trustzone/utils/tlutils
    ccflags-y += -I$(src)/$(MTK_PLATFORM)
    include $(srctree)/drivers/misc/mediatek/gud/Makefile.include
  endif
endif # !CONFIG_MTK_TEE_GP_SUPPORT

# Build as module
#obj-m += tz_module.o
#obj-y += tz_module.o
#tz_module-y += tz_mod.o tz_system.o test.o kree_mem.o tz_sys_ipc.o \
               tz_irq.o tz_pm.o tz_clock.o tz_ndbg.o tz_counter.o tz_mem.o tz_fileio.o tz_playready.o tz_secure_clock.o

obj-y	+= tz_mmap.o tz_system.o kree_mem.o
