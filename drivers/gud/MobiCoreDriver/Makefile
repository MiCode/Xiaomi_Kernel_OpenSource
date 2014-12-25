#
# this makefile is called from the kernel make system. Thus we basically
# add things to "obj-m" here.

ifeq ($(MODE),release)
    ccflags-y = -O2 -DNDEBUG
else
    ccflags-y = -DDEBUG
endif # DEBUG/RELEASE

# CFLAG for testable mode
ifeq ($(IS_TESTABLE),yes)
	ccflags-y = -DTEST
endif

# CFLAGS from the build script
ifdef MOBICORE_CFLAGS
	ccflags-y += $(MOBICORE_CFLAGS)
endif
#EXTRA_CFLAGS+=-DDEBUG_VERBOSE

ccflags-y += -Wall -D__$(PLATFORM)__
# add our module to kernel.
obj-m += mcDrvModule.o

mcDrvModule-objs :=logging.o ops.o mem.o api.o pm.o main.o

clean:
	rm -rf *.o *~ core .depend .*.cmd *.ko *.mod.c .tmp_versions \
		Module.markers Module.symvers modules.order

