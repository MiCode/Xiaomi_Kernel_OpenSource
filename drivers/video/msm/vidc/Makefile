ifdef CONFIG_MSM_VIDC_720P
EXTRA_CFLAGS += -Idrivers/video/msm/vidc/720p/ddl
EXTRA_CFLAGS += -Idrivers/video/msm/vidc/720p/resource_tracker
endif

ifdef CONFIG_MSM_VIDC_1080P
EXTRA_CFLAGS  += -Idrivers/video/msm/vidc/1080p/ddl
EXTRA_CFLAGS  += -Idrivers/video/msm/vidc/1080p/resource_tracker
endif

EXTRA_CFLAGS  += -Idrivers/video/msm/vidc/common/dec
EXTRA_CFLAGS  += -Idrivers/video/msm/vidc/common/enc
EXTRA_CFLAGS  += -Idrivers/video/msm/vidc/common/vcd
EXTRA_CFLAGS  += -Idrivers/video/msm/vidc/common/init

obj-$(CONFIG_MSM_VIDC) += vidc.o

vidc-objs :=	common/init/vidc_init.o \
		common/vcd/vcd_api.o \
		common/vcd/vcd_power_sm.o \
		common/vcd/vcd_client_sm.o \
		common/vcd/vcd_device_sm.o \
		common/vcd/vcd_scheduler.o \
		common/vcd/vcd_sub.o \

ifdef CONFIG_MSM_VIDC_720P
vidc-objs +=	720p/ddl/vcd_ddl_firmware.o \
		720p/ddl/vcd_ddl_metadata.o \
		720p/ddl/vidc.o \
		720p/ddl/vcd_ddl_utils.o \
		720p/ddl/vcd_ddl.o \
		720p/ddl/vcd_ddl_helper.o \
		720p/ddl/vcd_ddl_interrupt_handler.o \
		720p/ddl/vcd_ddl_hal.o \
		720p/ddl/vcd_ddl_properties.o \
		720p/resource_tracker/vcd_res_tracker.o \
		720p/ddl/vcd_ddl_errors.o
endif

ifdef CONFIG_MSM_VIDC_1080P
vidc-objs +=	1080p/ddl/vcd_ddl_helper.o \
		1080p/ddl/vcd_ddl_utils.o \
		1080p/ddl/vcd_ddl_interrupt_handler.o \
		1080p/ddl/vcd_ddl_properties.o \
		1080p/ddl/vcd_ddl_errors.o \
		1080p/ddl/vcd_ddl_shared_mem.o \
		1080p/ddl/vidc.o \
		1080p/ddl/vidc_pix_cache.o \
		1080p/ddl/vcd_ddl_vidc.o \
		1080p/ddl/vcd_ddl.o \
		1080p/ddl/vcd_ddl_metadata.o \
		1080p/resource_tracker/vcd_res_tracker.o
endif

obj-$(CONFIG_MSM_VIDC_VDEC) += vidc_vdec.o

vidc_vdec-objs :=	common/dec/vdec.o

obj-$(CONFIG_MSM_VIDC_VENC) += vidc_venc.o

vidc_venc-objs :=	common/enc/venc.o \
			common/enc/venc_internal.o
