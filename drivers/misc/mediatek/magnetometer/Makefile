# In case the platform does NOT support this type of sensors
ccflags-y += -I$(srctree)/drivers/misc/mediatek/hwmon/include
ccflags-y += -I$(srctree)/drivers/misc/mediatek/accelerometer/inc
ccflags-y += -I$(srctree)/drivers/misc/mediatek/magnetometer/inc
ifeq ($(CONFIG_CUSTOM_KERNEL_GYROSCOPE_MODULE),y)
obj-y   +=  mag_module.o
else
obj-y   +=  mag.o
endif
obj-y   +=  mag_factory.o

ifeq ($(CONFIG_MTK_SCP_SENSORHUB_V1),y)
obj-$(CONFIG_MTK_MAGHUB)        +=  maghub/
else
obj-$(CONFIG_MTK_AF7133)	+=  af7133/
obj-$(CONFIG_MTK_AKM8963)	+=  akm8963/
obj-$(CONFIG_MTK_AKM8963_NEW)	+=  akm8963-new/
obj-$(CONFIG_MTK_AKM8975)	+=  akm8975/
obj-$(CONFIG_MTK_AKM8975_AUTO)	+=  akm8975_auto/
obj-$(CONFIG_MTK_AKM09911)   +=  akm09911/
obj-$(CONFIG_MTK_AKM09912)   +=  akm09912/
obj-y   +=  akm09911-new/
obj-$(CONFIG_MTK_AMI304)   +=  ami304/
obj-$(CONFIG_MTK_AMI304_AUTO)   +=  ami304_auto/
obj-$(CONFIG_MTK_BMM050)	+=  bmm050/
obj-$(CONFIG_MTK_BMM050_NEW)	+=  bmm050-new/
obj-$(CONFIG_MTK_BMM056)	+=  bmm056/
obj-$(CONFIG_MTK_BMM056_NEW)	+=  bmm056-new/
obj-$(CONFIG_MTK_BMM056_NEW)	+=  bmm150/
obj-$(CONFIG_MTK_HSCDTD006)	+=  hscdtd006/
obj-$(CONFIG_MTK_HSCDTD006_AUTO)	+=  hscdtd006_auto/
obj-$(CONFIG_MTK_IST8303_NEW)	+=  ist8303-new/
obj-$(CONFIG_MTK_LSM303C_NEW)	+=  lsm303c-new/
obj-$(CONFIG_MTK_LSM303M)	+=  lsm303m/
obj-$(CONFIG_MTK_MAG3110)	+=  mag3110/
obj-$(CONFIG_MTK_MC41XX)	+=  mc41xx/
obj-$(CONFIG_MTK_MC64XX)	+=  mc64xx/
obj-$(CONFIG_MTK_MC6420)	+=  mc6420/
obj-$(CONFIG_MTK_MMC328X)	+=  mmc328x/
obj-$(CONFIG_MTK_MMC328X_AUTO)	+=  mmc328x_auto/
obj-$(CONFIG_MTK_MMC328XMA)	+=  mmc328xma/
obj-$(CONFIG_MTK_MMC3416X)	+=  mmc3416x/
obj-$(CONFIG_MTK_QMC5983)	+=  qmc5983/
obj-$(CONFIG_MTK_S62X)	+=  s62x/
obj-$(CONFIG_MTK_ST480)	+=  st480/
obj-$(CONFIG_MTK_YAS529)	+=  yamaha529/
obj-$(CONFIG_MTK_YAS530)	+=  yamaha530/
obj-$(CONFIG_MTK_YAS532)   +=  yamaha532/
obj-$(CONFIG_MTK_YAS532_NEW)   +=  yamaha532-new/
obj-$(CONFIG_MTK_YAS537)	+=  yamaha537/
obj-$(CONFIG_MTK_BMC156_MAG)	+=  bmc156_mag/
endif
