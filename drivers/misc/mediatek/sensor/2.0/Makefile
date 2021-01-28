# In case the platform does NOT support this type of sensors

ifneq ($(CONFIG_MTK_SENSOR_SUPPORT), n)
obj-y += core/

ifeq ($(CONFIG_CUSTOM_KERNEL_SENSORHUB),y)
obj-$(CONFIG_NANOHUB) += mtk_nanohub/
endif
obj-$(CONFIG_MTK_HF_LSM6DSM_SECONDARY) += lsm6dsm_secondary/
obj-$(CONFIG_MTK_HF_TEST_CASE) += test/
endif