include $(srctree)/drivers/misc/mediatek/Makefile.custom

ifeq ($(CONFIG_DEPRECATED_MTK_SYSTRACKER_SUPPORT),y)
  obj-y += systracker.o
  obj-y += backtrace.o
else
  obj-y += systracker_interface.o
  obj-y += $(subst ",,$(CONFIG_MTK_PLATFORM))/
  ifeq ($(CONFIG_ARM64), y)
    obj-y += backtrace_64bit.o
  else
    obj-y += backtrace32.o
    obj-y += backtrace32_asm.o
  endif
endif
