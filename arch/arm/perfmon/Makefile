obj-$(CONFIG_KSAPI) += ksapi.o

# Object file lists.
obj-y						+= perf-function-hooks.o
#ifdef CONFIG_ARCH_MSM8X60
ksapi-y						+= perf-v7.o perf-smp.o per.o per-process-perf.o per-axi.o
#else
ksapi-y						+= perf-v7.o per.o per-process-perf.o per-axi.o
#endif
