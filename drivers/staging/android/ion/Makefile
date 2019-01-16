ccflags-$(CONFIG_ION_MTK)  += -I$(srctree)/drivers/staging/android/ion 

obj-$(CONFIG_ION) +=	ion.o ion_heap.o ion_page_pool.o ion_system_heap.o \
			ion_carveout_heap.o ion_chunk_heap.o ion_cma_heap.o  ion_debug.o ion_profile.o
obj-$(CONFIG_ION_TEST) += ion_test.o
ifdef CONFIG_COMPAT
obj-$(CONFIG_ION) += compat_ion.o
endif
obj-$(CONFIG_ION_TEGRA) += tegra/

obj-$(CONFIG_ION_MTK) += mtk/

ifeq ($(CONFIG_MT_ENG_BUILD), y)
ccflags-y += -DION_RUNTIME_DEBUGGER=1
else
ccflags-y += -DION_RUNTIME_DEBUGGER=0
endif


