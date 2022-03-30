# debug KOs for ssc
ifeq ($(CONFIG_MTK_SSC_DBG_V1), m)
ifneq ($(wildcard $(srctree)/$(src)/v1),)
obj-$(CONFIG_MTK_SSC_DBG_V1) += v1/
endif
endif

ifeq ($(CONFIG_MTK_SSC_DBG_V2), m)
ifneq ($(wildcard $(srctree)/$(src)/v2),)
obj-$(CONFIG_MTK_SSC_DBG_V1) += v2/
endif
endif
