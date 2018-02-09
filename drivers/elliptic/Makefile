ccflags-y := -I$(src) -Wall -Werror
IO_MODULE := msm
MIXER_MODULE := test

EXTRA_CFLAGS += -DDEBUG

obj-y += elliptic.o io_modules/$(IO_MODULE)/elliptic_data_io.o elliptic_sysfs.o
# elliptic_driver-y := elliptic.o
# elliptic_driver-y += io_modules/$(IO_MODULE)/elliptic_data_io.o
# elliptic_driver-y += mixer_controls/$(MIXER_MODULE)/elliptic_mixer_controls.o
# elliptic_driver-y += elliptic_sysfs.o

# .PHONY: clean
# all:
# 	make -C /lib/modules/$(shell uname -r)/build/ M=$(PWD) modules
# clean:
# 	rm *.o *.ko
