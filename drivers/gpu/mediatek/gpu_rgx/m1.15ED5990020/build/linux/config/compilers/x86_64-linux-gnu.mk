# 64-bit x86 compiler
ifneq ($(KERNELDIR),)
 ifneq ($(ARCH),i386)
  ifeq ($(shell grep -q "CONFIG_X86_32=y" $(KERNELDIR)/.config && echo 1 || echo 0),1)
   $(warning ******************************************************)
   $(warning Your kernel appears to be configured for 32-bit x86,)
   $(warning but CROSS_COMPILE (or KERNEL_CROSS_COMPILE) points)
   $(warning to a 64-bit compiler.)
   $(warning If you want a 32-bit build, either set CROSS_COMPILE)
   $(warning to point to a 32-bit compiler, or build with ARCH=i386)
   $(warning to force 32-bit mode with your existing compiler.)
   $(warning ******************************************************)
   $(error Invalid CROSS_COMPILE / kernel architecture combination)
  endif # CONFIG_X86_32
 endif # ARCH=i386
endif # KERNELDIR

ifeq ($(ARCH),i386)
 # This is actually a 32-bit build using a native 64-bit compiler
 INCLUDE_I386-LINUX-GNU := true
else
 TARGET_PRIMARY_ARCH := target_x86_64
 ifeq ($(MULTIARCH),1)
  ifeq ($(CROSS_COMPILE_SECONDARY),)
   # The secondary architecture is being built with a native 64-bit compiler
   INCLUDE_I386-LINUX-GNU := true
  endif
 endif
endif

ifeq ($(INCLUDE_I386-LINUX-GNU),true)
 TARGET_FORCE_32BIT := -m32
 include $(compilers)/i386-linux-gnu.mk
endif
