include $(srctree)/drivers/misc/mediatek/Makefile.custom

#ccflags-y = -O0 -gdwarf-2
ccflags-y += -I$(srctree)/drivers/staging/android/ion

obj-y += m4u.o m4u_mva.o m4u_pgtable.o m4u_hw.o m4u_debug.o

ifeq ($(CONFIG_MMPROFILE),y)
ccflags-y += -DDEFAULT_MMP_ENABLE
endif
