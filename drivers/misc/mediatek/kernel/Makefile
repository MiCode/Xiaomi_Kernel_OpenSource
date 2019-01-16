include $(srctree)/drivers/misc/mediatek/Makefile.custom

ccflags-y += -I$(srctree)
obj-$(CONFIG_KGDB_KDB) += kdb_enhance.o
obj-$(CONFIG_MT65XX_TRACER) += trace_mt65xx_mon.o
obj-y += sched/
obj-$(CONFIG_MTK_MEMCFG) += mtk_memcfg.o
obj-$(CONFIG_TRACING) += mtk_trace.o
obj-y += mtk_meminfo.o
obj-$(CONFIG_MTK_MEMCFG) += mtk_memcfg.o
obj-y += system.o
obj-$(CONFIG_ARM) += mt_cache_v7.o
obj-$(CONFIG_ARM64) += mt_cache_v8.o
obj-y += fiq_smp_call.o
obj-y += sec_osal.o
