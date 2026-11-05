obj-$(CONFIG_MI_RT_MONITOR) += rt_mod.o
rt_mod-$(CONFIG_MI_RT_MONITOR) := rt.o rt-whitelist.o rt_load.o

ifeq ($(DEBUG), y)
CFLAGS_rt.o += -H
CFLAGS_rt-whitelist.o += -H
endif


ifeq ($(GCOV), y)
  GCOV_PROFILE := y
  KERNELBUILD := /sys/kernel/debug/gcov/tmp/ko
else
  KERNELBUILD := /lib/modules/$(shell uname -r)/build
endif

ifeq ("$(origin O)", "command line")
  OUTPUTDIR = $(O)
endif
ifndef OUTPUTDIR
  OUTPUTDIR =
endif

default: rt_mod test
ifeq ($(OUTPUTDIR),)
rt_mod:
	rm -rf .tmp_versions *.ko.unsigned  modules.order
	rm -f *.o .*.cmd *.ko *.mod *.mod.c  Module.markers  Module.symvers
ifeq ($(VERBOSE), y)
	make V=1 -C $(KERNELBUILD) M=$(shell pwd) modules
else
	make -C $(KERNELBUILD) M=$(shell pwd) modules
endif

else
rt_mod:
	rm -rf $(OUTPUTDIR)
	rm -rf .tmp_versions *.ko.unsigned  modules.order
	rm -f *.o .*.cmd *.ko *.mod *.mod.c  Module.markers  Module.symvers
	make -C $(KERNELBUILD) M=$(shell pwd) modules
	mkdir -p $(OUTPUTDIR)
	mv *.ko ./$(OUTPUTDIR)
endif

test: schedulerTest.o
	gcc -o schedulerTest schedulerTest.o
clean:
	rm -rf .tmp_versions *.ko.unsigned  modules.order
	rm -f *.o .*.cmd *.ko *.mod *.mod.c  Module.markers  Module.symvers
	rm -rf schedulerTest.o schedulerTest

ifeq ($(GCOV), y)
	rm -fr *.gcda *.gcno .*.gcno
endif

