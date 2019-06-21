ccflags-y := -I$(src) -Wall -Werror
IO_MODULE := msm

# EXTRA_CFLAGS += -DDEBUG

# Need to add include paths:
LINUXINCLUDE += \
             -I$(srctree)techpack/audio/include/dsp
             -I$(srctree)techpack/audio/include/uapi
             -I$(srctree)techpack/aduio/include

obj-y += elliptic.o
obj-y += elliptic_mixer_controls.o
obj-y += io_modules/$(IO_MODULE)/elliptic_data_$(IO_MODULE)_io.o
obj-y += io_modules/userspace/elliptic_data_userspace_io.o
obj-y += io_modules/userspace/elliptic_data_userspace_ctrl.o
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