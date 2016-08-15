#
# Makefile for Invensense IIO compass drivers of 3rd party compass devices.
#

# Yamaha YAS530/YAS532/YAS533
obj-$(CONFIG_INV_YAS53X_IIO) += inv_yas53x.o

inv_yas53x-objs := inv_yas53x_core.o
inv_yas53x-objs += inv_yas53x_ring.o
inv_yas53x-objs += inv_yas53x_trigger.o

CFLAGS_inv_yas53x_core.o    += -Idrivers/staging/iio
CFLAGS_inv_yas53x_ring.o    += -Idrivers/staging/iio
CFLAGS_inv_yas53x_trigger.o += -Idrivers/staging/iio

# Aichi AMI306
obj-$(CONFIG_INV_AMI306_IIO) += inv-ami306-iio.o

inv-ami306-iio-objs := inv_ami306_core.o
inv-ami306-iio-objs += inv_ami306_ring.o
inv-ami306-iio-objs += inv_ami306_trigger.o

CFLAGS_inv_ami306_core.o    += -Idrivers/staging/iio
CFLAGS_inv_ami306_ring.o    += -Idrivers/staging/iio
CFLAGS_inv_ami306_trigger.o += -Idrivers/staging/iio
