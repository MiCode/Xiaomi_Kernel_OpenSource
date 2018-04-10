ccflags-y += -I$(src)			# needed for trace events

obj-y					+= ion/
obj-$(CONFIG_FIQ_DEBUGGER)		+= fiq_debugger/

obj-$(CONFIG_ANDROID_BINDER_IPC)	+= binder.o binder_alloc.o
obj-$(CONFIG_ANDROID_BINDER_IPC_SELFTEST) += binder_alloc_selftest.o
obj-$(CONFIG_ASHMEM)			+= ashmem.o
obj-$(CONFIG_ANDROID_LOGGER)           += logger.o
obj-$(CONFIG_ANDROID_TIMED_OUTPUT)	+= timed_output.o
obj-$(CONFIG_ANDROID_TIMED_GPIO)	+= timed_gpio.o
obj-$(CONFIG_ANDROID_WHETSTONE)	+= memsw_state.o
obj-$(CONFIG_ANDROID_LOW_MEMORY_KILLER)	+= lowmemorykiller.o
obj-$(CONFIG_SYNC)			+= sync.o
obj-$(CONFIG_SW_SYNC)			+= sw_sync.o
obj-$(CONFIG_ONESHOT_SYNC)		+= oneshot_sync.o
obj-$(CONFIG_SWAPAGING)			+= swapaging.o
