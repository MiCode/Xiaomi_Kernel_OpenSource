ccflags-y += -I$(srctree)/drivers/misc/mediatek/sensors-1.0/sensorfusion
ccflags-y += -I$(srctree)/drivers/misc/mediatek/sensors-1.0/hwmon/include
ccflags-y += -I$(srctree)/drivers/misc/mediatek/sensors-1.0/sensorHub/inc_v1
ccflags-y += -I$(srctree)/drivers/misc/mediatek/scp/

obj-$(CONFIG_MTK_SENSOR_SUPPORT) += fusionhub.o
fusionhub-y := fusion_hub.o
fusionhub-$(CONFIG_MTK_ROTATVECHUB)	+= rotatvechub/rotatvechub.o
fusionhub-$(CONFIG_MTK_GAMEROTVECHUB) += gamerotvechub/gamerotvechub.o
fusionhub-$(CONFIG_MTK_GMAGROTVECHUB) += gmagrotvechub/gmagrotvechub.o
fusionhub-$(CONFIG_MTK_GRAVITYHUB) += gravityhub/gravityhub.o
fusionhub-$(CONFIG_MTK_LINEARACCHUB) += linearacchub/linearacchub.o
fusionhub-$(CONFIG_MTK_ORIENTHUB) += orienthub/orienthub.o
fusionhub-$(CONFIG_MTK_UNCALI_ACCHUB) += uncali_acchub/uncali_acchub.o
fusionhub-$(CONFIG_MTK_UNCALI_GYROHUB) += uncali_gyrohub/uncali_gyrohub.o
fusionhub-$(CONFIG_MTK_UNCALI_MAGHUB) += uncali_maghub/uncali_maghub.o