include $(srctree)/drivers/misc/mediatek/Makefile.custom

# Linux driver folder
ccflags-y += -I$(srctree)/arch/arm/mach-$(MTK_PLATFORM)/$(ARCH_MTK_PROJECT)/touchpanel/FTS2A052/
ccflags-y += -I$(srctree)/drivers/input/touchscreen/mediatek/FTS2A052/
ccflags-y += -I$(srctree)/drivers/input/touchscreen/mediatek/

obj-y	+=  fts2a052_driver.o

