include $(srctree)/drivers/misc/mediatek/Makefile.custom

# In case the platform does NOT support this type of sensors
obj-y   +=  gyroscope.o gyro_factory.o

ifeq ($(CONFIG_MTK_MPU3050C),y)
obj-y	+=  mpu3050c/
endif

ifeq ($(CONFIG_MTK_MPU6050C),y)
obj-y	+=  mpu6050c/
endif

ifeq ($(CONFIG_MTK_MPU3000),y)
obj-y   +=  mpu3000/
endif

ifeq ($(CONFIG_MTK_MPU3000_NEW),y)
obj-y   +=  mpu3000-new/
endif
ifeq ($(CONFIG_MTK_MPU6050GY_NEW),y)
obj-y   +=  mpu6050gy-new/
endif

obj-y   +=  bmi160_gyro/

obj-y   +=  lsm6ds3/

ifeq ($(CONFIG_MTK_MPU6515G),y)
obj-y   +=  mpu6515/
endif

ifeq ($(CONFIG_MTK_MPU3050C_NEW),y)
obj-y   +=  mpu3050c-new/
endif
ifeq ($(CONFIG_MTK_ITG1010_NEW),y)
obj-y   +=  ITG1010-new/
endif
