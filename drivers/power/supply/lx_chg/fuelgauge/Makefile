obj-$(CONFIG_BQ_FUELGAUGE)				+= fg_bq28z610.o
fg_bq28z610-objs := bq28z610.o
ifeq ($(CONFIG_LX_ADAPTIVE_POWEROFF_VOLTAGE),m)
fg_bq28z610-objs += lx_adaptive_poweroff_voltage.o
endif
obj-$(CONFIG_CHARGE_PARTITION)			+= charger_partition.o
