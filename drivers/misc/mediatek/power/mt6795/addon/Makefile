ccflags-y +=-I$(srctree)/drivers/misc/mediatek/power/$(MTK_PLATFORM)

ifeq ($(CONFIG_MTK_BQ24196_SUPPORT),y) 
    obj-$(CONFIG_MTK_SMART_BATTERY) += mt_battery_bq24196.o mt6320_fuel_gauge_bq24196.o bq24196.o    
    ifeq ($(CONFIG_MTK_BQ27541_SUPPORT),y)
      obj-$(CONFIG_MTK_SMART_BATTERY) += bq27541.o      
    endif
else
  ifeq ($(CONFIG_MTK_NCP1851_SUPPORT),y) 
    obj-$(CONFIG_MTK_SMART_BATTERY) += mt_battery_ncp1851.o mt6320_fuel_gauge_ncp1851.o ncp1851.o
  endif
endif
