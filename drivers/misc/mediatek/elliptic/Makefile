# SPDX-License-Identifier: GPL-2.0


# ccflags-y := -I$(src) -Wall -Werror
ccflags-y := -I$(src) -Wall
IO_MODULE := mtk

# EXTRA_CFLAGS += -DDEBUG

ifeq ($(IO_MODULE), mtk)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/scp/rv
ccflags-y += -I$(srctree)/drivers/misc/mediatek/scp/include
ccflags-y += -I$(srctree)/drivers/misc/mediatek/audio_ipi/common_v2/framework
ccflags-y += -I$(srctree)/drivers/misc/mediatek/sensors-1.0/hwmon/include
ccflags-y += -I$(srctree)/drivers/misc/mediatek/sensors-1.0/sensorHub/inc_v1
ccflags-y += -I$(srctree)/drivers/misc/mediatek/sensor/2.0/core
endif

obj-y += elliptic.o
obj-y += elliptic_mixer_controls.o
obj-y += io_modules/$(IO_MODULE)/elliptic_data_$(IO_MODULE)_io.o
#obj-y += io_modules/userspace/elliptic_data_userspace_io.o
#obj-y += io_modules/userspace/elliptic_data_userspace_ctrl.o
obj-y += elliptic_sysfs.o

# elliptic_driver-y := elliptic.o
# elliptic_driver-y += io_modules/$(IO_MODULE)/elliptic_data_io.o
# elliptic_driver-y += mixer_controls/$(MIXER_MODULE)/elliptic_mixer_controls.o
# elliptic_driver-y += elliptic_sysfs.o

# .PHONY: clean
# all:
# 	make -C /lib/modules/$(shell uname -r)/build/ M=$(PWD) modules
# clean:
# 	rm *.o *.ko
