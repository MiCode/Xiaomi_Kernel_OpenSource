# In case the platform does NOT support this type of sensors
ccflags-y += -I$(srctree)/drivers/misc/mediatek/hwmon/include

ifeq ($(CONFIG_CUSTOM_KERNEL_ACCELEROMETER_MODULE),y)
obj-y   +=  accel_module.o accel_factory.o
else
obj-y   +=  accel.o accel_factory.o
endif

ifeq ($(CONFIG_MTK_SCP_SENSORHUB_V1),y)
obj-$(CONFIG_MTK_ACCELHUB)		+=	accelhub/
else
obj-$(CONFIG_MTK_K2DH)   +=  k2dh/
obj-$(CONFIG_MTK_BMA050)   +=  bma050/
obj-$(CONFIG_MTK_BMA2XX)   +=  bma2xx/
obj-$(CONFIG_MTK_BMA050_NEW)   +=  bma050-new/
obj-$(CONFIG_MTK_BMA222E)   +=  bma222E/
obj-$(CONFIG_MTK_BMA222E_NEW)   +=  bma222E-new/
obj-$(CONFIG_MTK_MPU6050G)   +=  mpu6050g/
obj-$(CONFIG_MTK_MPU6050G_NEW)   +=  mpu6050g-new/
obj-$(CONFIG_MTK_BMA250)   +=  bma250/
obj-$(CONFIG_MTK_KXTIK1004)   +=  kxtik1004/
obj-$(CONFIG_MTK_MPU6515A)   +=  mpu6515/
obj-$(CONFIG_MTK_KXTJ2_1009)   +=  KXTJ2_1009/
obj-$(CONFIG_MTK_KXTJ2_1009_NEW)   +=  KXTJ2_1009-new/
obj-$(CONFIG_MTK_BMA250E)   +=  bma250e/
obj-y   +=  mc3410-new/
obj-$(CONFIG_MTK_MC3XXX_AUTO)   +=  mc3xxx_auto/
obj-$(CONFIG_MTK_MPU60X0)   +=  mpu60x0/
obj-$(CONFIG_MTK_BMA255_SDO0)   +=  bma255-sdo0/
obj-$(CONFIG_MTK_BMA255_SDO1)   +=  bma255-sdo1/
obj-$(CONFIG_MTK_BMA056)   +=  bma056/
obj-$(CONFIG_MTK_LSM6DS3)   +=  lsm6ds3/
obj-$(CONFIG_MTK_ICM20645G)   +=  icm20645g/
obj-$(CONFIG_MTK_BMC156_ACC)   +=  bmc156_acc/
obj-$(CONFIG_MTK_MMA8653)   +=  mma8653/
obj-$(CONFIG_MTK_ACCEL_RT3000)   +=  rt3000/
obj-$(CONFIG_MTK_LSM6DS3HG)   +=  lsm6ds3hg/
obj-$(CONFIG_MTK_BMI160_NEW)   +=  bmi160/
endif
