obj-$(CONFIG_ION) +=	ion.o ion-ioctl.o ion_heap.o \
			ion_page_pool.o ion_system_heap.o \
			ion_carveout_heap.o ion_chunk_heap.o ion_cma_heap.o
obj-$(CONFIG_ION_TEST) += ion_test.o
ifdef CONFIG_COMPAT
obj-$(CONFIG_ION) += compat_ion.o
endif

obj-$(CONFIG_ION_DUMMY) += ion_dummy_driver.o
obj-$(CONFIG_ION_OF) += ion_of.o

obj-$(CONFIG_MTK_ION) += mtk/
ccflags-$(CONFIG_MTK_ION)  += -I$(srctree)/drivers/staging/android/mtk_ion/mtk
ccflags-y += -I$(srctree)/drivers/misc/mediatek/mmp/
ccflags-$(CONFIG_MTK_AEE_FEATURE) += -I$(srctree)/drivers/misc/mediatek/include/mt-plat
ccflags-y += -DION_HISTORY_RECORD
ifeq ($(CONFIG_MTK_ENG_BUILD), y)
ccflags-y += -DION_RUNTIME_DEBUGGER=1
else
ccflags-y += -DION_RUNTIME_DEBUGGER=0
endif
