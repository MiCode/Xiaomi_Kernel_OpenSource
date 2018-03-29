ccflags-y += -I$(srctree)/drivers/misc/mediatek/usb_c/fusb302

obj-y += sii70xxdrv.o
sii70xxdrv-objs += ./Platform_Linux/sii_main.o
sii70xxdrv-objs += ./Platform_Linux/Wrap.o
sii70xxdrv-objs += ./Common/sii70xxdrv.o
sii70xxdrv-objs += ./Common/si_usbpd_sysfs.o
sii70xxdrv-objs += ./Common/si_time.o
sii70xxdrv-objs += ./Common/si_phy.o
sii70xxdrv-objs += ./Common/si_usbpd_dfp.o
sii70xxdrv-objs += ./Common/si_usbpd_ufp.o
sii70xxdrv-objs += ./Common/si_usbpd.o
sii70xxdrv-objs += ./Common/si_platform.o
sii70xxdrv-objs += ./Common/si_usbpd_core.o

sii70xxdrv-objs += ./Common/sii70xx_switch.o
ccflags-y +=-DSII_LINUX_BUILD=\"$(SII_LINUX_BUILD)\"
EXTRA_CFLAGS := -I$(src)/Platform_Linux

