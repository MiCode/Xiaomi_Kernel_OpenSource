ifeq ($(CONFIG_SND_SOC_DBMDX),y)
	obj-$(CONFIG_SND_SOC_DBMDX) := dbmdx.o
	obj-$(CONFIG_SND_SOC_DBMDX) += dbmdx-spi.o
	obj-$(CONFIG_SND_SOC_DBMDX) += dbmdx-i2c.o
	obj-$(CONFIG_SND_SOC_DBMDX) += dbmdx-i2c-d2.o
	obj-$(CONFIG_SND_SOC_DBMDX) += dbmdx-i2c-d4.o
	obj-$(CONFIG_SND_SOC_DBMDX) += dbmdx-spi-d2.o
	obj-$(CONFIG_SND_SOC_DBMDX) += dbmdx-spi-d4.o
	obj-$(CONFIG_SND_SOC_DBMDX) += dbmdx-uart-d2.o
	obj-$(CONFIG_SND_SOC_DBMDX) += dbmdx-uart-d4.o
	obj-$(CONFIG_SND_SOC_DBMDX) += dbmdx-uart.o
	obj-$(CONFIG_SND_SOC_DBMDX) += dbmdx-i2s.o
	obj-$(CONFIG_SND_SOC_DBMDX) += dbmdx-customer.o
	obj-$(CONFIG_SND_SOC_DBMDX) += dbmdx-cdev.o
	obj-$(CONFIG_SND_SOC_DBMDX_SND_CAPTURE) += dbmdx-snd.o
	obj-$(CONFIG_SND_SOC_DBMDX_SND_CAPTURE) += dbmdx-snd-pcm.o
else
	ifeq ($(CONFIG_SND_SOC_DBMDX),m)
		obj-$(CONFIG_SND_SOC_DBMDX) := dbmdx_codec.o

		dbmdx_codec-objs := dbmdx.o dbmdx-customer.o dbmdx-cdev.o dbmdx-i2s.o

		#DBMD4
		ifneq ($(CONFIG_SND_SOC_DBMD4_SPI),)
			dbmdx_codec-objs += dbmdx-spi.o
			dbmdx_codec-objs += dbmdx-spi-d4.o
		endif

		ifneq ($(CONFIG_SND_SOC_DBMD4_I2C),)
			dbmdx_codec-objs += dbmdx-i2c.o
			dbmdx_codec-objs += dbmdx-i2c-d4.o
		endif

		ifneq ($(CONFIG_SND_SOC_DBMD4_UART),)
			dbmdx_codec-objs += dbmdx-uart.o
			dbmdx_codec-objs += dbmdx-uart-d4.o
		endif

		#DBMD2
		ifneq ($(CONFIG_SND_SOC_DBMD2_SPI),)
			dbmdx_codec-objs += dbmdx-spi.o
			dbmdx_codec-objs += dbmdx-spi-d2.o
		endif

		ifneq ($(CONFIG_SND_SOC_DBMD2_I2C),)
			dbmdx_codec-objs += dbmdx-i2c.o
			dbmdx_codec-objs += dbmdx-i2c-d2.o
		endif

		ifneq ($(CONFIG_SND_SOC_DBMD2_UART),)
			dbmdx_codec-objs += dbmdx-uart.o
			dbmdx_codec-objs += dbmdx-uart-d2.o
		endif

		ifeq ($(CONFIG_SND_SOC_DBMDX_SND_CAPTURE),y)
			dbmdx_codec-objs += dbmdx-snd.o
			dbmdx_codec-objs += dbmdx-snd-pcm.o
		endif
	endif
endif

ifneq ($(KERNELRELEASE),)
# kbuild part of makefile
else
# normal makefile
all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) \
	KBUILD_EXTMOD=$(PWD) \
	modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
endif
