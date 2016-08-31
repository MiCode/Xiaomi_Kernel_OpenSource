#
# Makefile for Invensense IIO compass drivers of 3rd party compass devices.
#

# Yamaha YAS530/YAS532/YAS533
obj-$(CONFIG_INV_YAS53X_IIO) += inv_yas53x.o

inv_yas53x-objs := inv_yas53x_core.o
inv_yas53x-objs += inv_yas53x_ring.o
inv_yas53x-objs += inv_yas53x_trigger.o

CFLAGS_inv_yas53x_core.o    += -Idrivers/iio
CFLAGS_inv_yas53x_ring.o    += -Idrivers/iio
CFLAGS_inv_yas53x_trigger.o += -Idrivers/iio

# Aichi AMI306
obj-$(CONFIG_INV_AMI306_IIO) += inv-ami306-iio.o

inv-ami306-iio-objs := inv_ami306_core.o
inv-ami306-iio-objs += inv_ami306_ring.o
inv-ami306-iio-objs += inv_ami306_trigger.o

CFLAGS_inv_ami306_core.o    += -Idrivers/iio
CFLAGS_inv_ami306_ring.o    += -Idrivers/iio
CFLAGS_inv_ami306_trigger.o += -Idrivers/iio

# Asahi Kasei AK8975/AK8972/AK8963
obj-$(CONFIG_INV_AK89XX_IIO) += inv-ak89xx-iio.o

inv-ak89xx-iio-objs := inv_ak89xx_core.o
inv-ak89xx-iio-objs += inv_ak89xx_ring.o
inv-ak89xx-iio-objs += inv_ak89xx_trigger.o

CFLAGS_inv_ak89xx_core.o    += -Idrivers/iio
CFLAGS_inv_ak89xx_ring.o    += -Idrivers/iio
CFLAGS_inv_ak89xx_trigger.o += -Idrivers/iio