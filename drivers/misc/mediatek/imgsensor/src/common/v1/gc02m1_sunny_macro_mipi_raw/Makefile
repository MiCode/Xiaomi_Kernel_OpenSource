IMGSENSOR_SUBDRV_OBJ := \
	$(subdrv-rpath)/gc02m1_sunny_macro_mipi_raw.o \

ifeq ($(CONFIG_MTK_IMGSENSOR_ISP4), m)
$(eval $(MTK_PLATFORM_KO)-objs += $(IMGSENSOR_SUBDRV_OBJ))
else ifeq ($(CONFIG_MTK_IMGSENSOR_ISP4), y)
obj-$(CONFIG_MTK_IMGSENSOR_ISP4) += $(IMGSENSOR_SUBDRV_OBJ)
endif
