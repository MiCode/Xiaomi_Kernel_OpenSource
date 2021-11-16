ccflags-y := -I$(src) -Wall -Werror
IO_MODULE := msm

# EXTRA_CFLAGS += -DDEBUG

# Need to add include paths:
LINUXINCLUDE += \
             -I$(srctree)techpack/audio/include/dsp
             -I$(srctree)techpack/audio/include/uapi
             -I$(srctree)techpack/aduio/include

obj-y += mius.o
obj-y += mius_mixer_controls.o
obj-y += io_modules/$(IO_MODULE)/mius_data_$(IO_MODULE)_io.o
obj-y += io_modules/userspace/mius_data_userspace_io.o
obj-y += io_modules/userspace/mius_data_userspace_ctrl.o
obj-y += mius_sysfs.o

# mius_driver-y := mius.o
# mius_driver-y += io_modules/$(IO_MODULE)/mius_data_io.o
# mius_driver-y += mixer_controls/$(MIXER_MODULE)/mius_mixer_controls.o
# mius_driver-y += mius_sysfs.o

# .PHONY: clean
# all:
# 	make -C /lib/modules/$(shell uname -r)/build/ M=$(PWD) modules
# clean:
# 	rm *.o *.ko
