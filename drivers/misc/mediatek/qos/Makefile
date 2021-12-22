
ifneq (,$(filter $(CONFIG_MTK_PLATFORM),  "mt6785" "mt6853" "mt6768" "mt6885" "mt6893"))
else
ccflags-y += -I$(srctree)/drivers/misc/mediatek/sspm/

obj-$(CONFIG_MTK_QOS_FRAMEWORK) += mtk_qos.o
mtk_qos-objs := qos-v79.o mtk_qos_common.o  mtk_qos_bound.o  mtk_qos_sysfs.o

ifneq ($(CONFIG_MACH_MT6761),)
obj-$(CONFIG_MTK_QOS_FRAMEWORK) += mtk_qos_legacy.o
mtk_qos_legacy-objs := qos-v61.o
endif

ifneq ($(CONFIG_MACH_MT6765),)
obj-$(CONFIG_MTK_QOS_FRAMEWORK) += mtk_qos_legacy.o
mtk_qos_legacy-objs := qos-v61.o
endif

endif
