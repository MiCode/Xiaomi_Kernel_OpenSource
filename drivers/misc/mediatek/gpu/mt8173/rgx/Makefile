#include $(srctree)/drivers/misc/mediatek/Makefile.custom

OPTIM = -Os


RGX_TOP := $(srctree)/drivers/misc/mediatek/gpu/$(MTK_PLATFORM)/rgx

$(info *RGX_TOP = $(RGX_TOP))

ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/$(MTK_PLATFORM)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/met/$(MTK_PLATFORM)/platform/$(MTK_PLATFORM)/


VER := $(if $(filter eng,$(TARGET_BUILD_VARIANT)),eng,user)

#MTK_DDK_DEBUG := y

ifeq ($(MTK_DDK_DEBUG),y)
ccflags-y += -include $(RGX_TOP)/include/config_kernel_debug.h
else
ccflags-y += -include $(RGX_TOP)/include/config_kernel_$(VER).h
endif

ccflags-y += \
 -D__linux__ $(OPTIM) -g \
 -fno-strict-aliasing \
 -W -Wall \
 -Wno-missing-field-initializers \
 -Wdeclaration-after-statement \
 -Wno-format-zero-length \
 -Wmissing-prototypes \
 -Wstrict-prototypes \
 -Wno-unused-parameter \
 -Wno-sign-compare \
 -Wno-type-limits \
 -Wno-error

pvrsrvkm-y += \
 services/server/env/linux/event.o \
 services/server/env/linux/km_apphint.o \
 services/server/env/linux/module_common.o \
 services/server/env/linux/osfunc.o \
 services/server/env/linux/osmmap_stub.o \
 services/server/env/linux/allocmem.o \
 services/server/env/linux/pdump.o \
 services/server/env/linux/osconnection_server.o \
 services/server/env/linux/pmr_os.o \
 services/server/env/linux/physmem_osmem_linux.o \
 services/server/env/linux/pvr_debugfs.o \
 services/server/env/linux/pvr_bridge_k.o \
 services/server/env/linux/physmem_dmabuf.o \
 services/server/env/linux/pvr_debug.o \
 services/server/env/linux/handle_idr.o \
 services/server/env/linux/pvr_gputrace.o \
 services/server/env/linux/osfunc_arm64.o \
 services/server/env/linux/trace_events.o \
 services/server/common/devicemem_heapcfg.o \
 services/server/common/devicemem_server.o \
 services/server/common/physheap.o \
 services/server/common/handle.o \
 services/server/common/mmu_common.o \
 services/server/common/lists.o \
 services/server/common/connection_server.o \
 services/server/common/physmem.o \
 services/server/common/physmem_lma.o \
 services/server/common/physmem_tdsecbuf.o \
 services/server/common/pmr.o \
 services/server/common/power.o \
 services/server/common/process_stats.o \
 services/server/common/pvr_notifier.o \
 services/server/common/pvrsrv.o \
 services/server/common/sync_server.o \
 services/server/common/srvcore.o \
 services/server/common/tlintern.o \
 services/server/common/sync_checkpoint.o \
 services/server/common/tlserver.o \
 services/server/common/tlstream.o \
 services/server/common/htbserver.o \
 services/server/common/cache_rangebased.o \
 services/server/devices/rgx/debugmisc_server.o \
 services/server/devices/rgx/rgxccb.o \
 services/server/devices/rgx/rgxdebug.o \
 services/server/devices/rgx/rgxfwutils.o \
 services/server/devices/rgx/rgxinit.o \
 services/server/devices/rgx/rgxkicksync.o \
 services/server/devices/rgx/rgxbreakpoint.o \
 services/server/devices/rgx/rgxlayer_km_impl.o \
 services/server/devices/rgx/rgxmem.o \
 services/server/devices/rgx/rgxmmuinit.o \
 services/server/devices/rgx/rgxsync.o \
 services/server/devices/rgx/rgxregconfig.o \
 services/server/devices/rgx/rgxtransfer.o \
 services/server/devices/rgx/rgxutils.o \
 services/server/devices/rgx/rgxta3d.o \
 services/server/devices/rgx/rgxtimerquery.o \
 services/server/devices/rgx/rgxtdmtransfer.o \
 services/server/devices/rgx/rgxpower.o \
 services/server/devices/rgx/rgxhwperf.o \
 services/server/devices/rgx/rgxstartstop.o \
 services/server/devices/rgx/rgxtimecorr.o \
 services/server/devices/rgx/rgxcompute.o \
 services/shared/common/devicemem.o \
 services/shared/common/hash.o \
 services/shared/common/ra.o \
 services/shared/common/sync.o \
 services/shared/common/mem_utils.o \
 services/shared/common/devicemem_utils.o \
 services/shared/common/htbuffer.o \
 services/shared/common/tlclient.o \
 services/shared/common/uniq_key_splay_tree.o \
 services/shared/devices/rgx/rgx_compat_bvnc.o \
 services/shared/devices/rgx/rgx_hwperf_table.o \
 services/srvinit/common/srvinit_pdump.o \
 services/srvinit/devices/rgx/env/linux/km/rgxfwload.o \
 services/srvinit/devices/rgx/rgxlayer_impl.o \
 services/srvinit/devices/rgx/rgxfwimageutils.o \
 services/srvinit/devices/rgx/rgxsrvinit_script.o \
 services/srvinit/devices/rgx/rgxsrvinit.o \
 services/system/rgx_mtk/ion_support.o \
 services/system/rgx_mtk/mtk_mfgsys.o \
 services/system/rgx_mtk/mtk_pp.o \
 services/system/rgx_mtk/sysconfig.o \
 services/system/common/env/linux/interrupt_support.o \
 kernel/drivers/staging/imgtec/pvr_platform_drv.o \
 kernel/drivers/staging/imgtec/pvr_sync.o \
 kernel/drivers/staging/imgtec/pvr_drm.o \
 generated/mm_bridge/client_mm_direct_bridge.o \
 generated/mm_bridge/server_mm_bridge.o \
 generated/cmm_bridge/server_cmm_bridge.o \
 generated/rgxtq_bridge/server_rgxtq_bridge.o \
 generated/rgxtq2_bridge/server_rgxtq2_bridge.o \
 generated/rgxta3d_bridge/server_rgxta3d_bridge.o \
 generated/srvcore_bridge/server_srvcore_bridge.o \
 generated/sync_bridge/client_sync_direct_bridge.o \
 generated/sync_bridge/server_sync_bridge.o \
 generated/breakpoint_bridge/server_breakpoint_bridge.o \
 generated/debugmisc_bridge/server_debugmisc_bridge.o \
 generated/htbuffer_bridge/server_htbuffer_bridge.o \
 generated/htbuffer_bridge/client_htbuffer_direct_bridge.o \
 generated/pvrtl_bridge/client_pvrtl_direct_bridge.o \
 generated/pvrtl_bridge/server_pvrtl_bridge.o \
 generated/rgxhwperf_bridge/server_rgxhwperf_bridge.o \
 generated/regconfig_bridge/server_regconfig_bridge.o \
 generated/timerquery_bridge/server_timerquery_bridge.o \
 generated/rgxkicksync_bridge/server_rgxkicksync_bridge.o \
 generated/cacherangebased_bridge/client_cacherangebased_direct_bridge.o \
 generated/cacherangebased_bridge/server_cacherangebased_bridge.o \
 generated/dmabuf_bridge/server_dmabuf_bridge.o \
 generated/rgxcmp_bridge/server_rgxcmp_bridge.o \
 generated/rgxinit_bridge/client_rgxinit_direct_bridge.o
 
ifeq ($(MTK_DDK_DEBUG),y)
pvrsrvkm-y += \
 services/server/common/ri_server.o \
 generated/ri_bridge/client_ri_direct_bridge.o \
 generated/ri_bridge/server_ri_bridge.o \
 generated/devicememhistory_bridge/server_devicememhistory_bridge.o \
 generated/devicememhistory_bridge/client_devicememhistory_direct_bridge.o \
 services/server/common/devicemem_history_server.o
endif


pvrsrvkm-$(CONFIG_ARM) += services/server/env/linux/osfunc_arm.o
pvrsrvkm-$(CONFIG_ARM64) += services/server/env/linux/osfunc_arm64.o

pvrsrvkm-$(CONFIG_EVENT_TRACING) += services/server/env/linux/trace_events.o

ccflags-y += \
 -I$(srctree)/drivers/staging/android/ion \
 -I$(srctree)/drivers/misc/mediatek/gpu/ged/include \
 -I$(srctree)/drivers/misc/mediatek/include

ifeq ($(CONFIG_MTK_GPU_COMMON_DVFS_SUPPORT),y)
ccflags-y += -DENABLE_COMMON_DVFS
endif

ccflags-y += \
 -I$(RGX_TOP)/generated/breakpoint_bridge \
 -I$(RGX_TOP)/generated/cacherangebased_bridge \
 -I$(RGX_TOP)/generated/cmm_bridge \
 -I$(RGX_TOP)/generated/debugmisc_bridge \
 -I$(RGX_TOP)/generated/dmabuf_bridge \
 -I$(RGX_TOP)/generated/htbuffer_bridge \
 -I$(RGX_TOP)/generated/mm_bridge \
 -I$(RGX_TOP)/generated/pvrtl_bridge \
 -I$(RGX_TOP)/generated/regconfig_bridge \
 -I$(RGX_TOP)/generated/rgxcmp_bridge \
 -I$(RGX_TOP)/generated/rgxhwperf_bridge \
 -I$(RGX_TOP)/generated/rgxinit_bridge \
 -I$(RGX_TOP)/generated/rgxkicksync_bridge \
 -I$(RGX_TOP)/generated/rgxta3d_bridge \
 -I$(RGX_TOP)/generated/rgxtq_bridge \
 -I$(RGX_TOP)/generated/rgxtq2_bridge \
 -I$(RGX_TOP)/generated/srvcore_bridge \
 -I$(RGX_TOP)/generated/sync_bridge \
 -I$(RGX_TOP)/generated/timerquery_bridge \
 -I$(RGX_TOP)/hwdefs \
 -I$(RGX_TOP)/hwdefs/km \
 -I$(RGX_TOP)/hwdefs/km/configs \
 -I$(RGX_TOP)/hwdefs/km/cores \
 -I$(RGX_TOP)/include \
 -I$(RGX_TOP)/include/drm \
 -I$(RGX_TOP)/kernel/drivers/staging/imgtec \
 -I$(RGX_TOP)/services/include \
 -I$(RGX_TOP)/services/include/env/linux \
 -I$(RGX_TOP)/services/include/shared \
 -I$(RGX_TOP)/services/server/devices/rgx \
 -I$(RGX_TOP)/services/server/env/linux \
 -I$(RGX_TOP)/services/server/include \
 -I$(RGX_TOP)/services/shared/common \
 -I$(RGX_TOP)/services/shared/devices/rgx \
 -I$(RGX_TOP)/services/shared/include \
 -I$(RGX_TOP)/services/srvinit/devices/rgx \
 -I$(RGX_TOP)/services/srvinit/include \
 -I$(RGX_TOP)/services/system/include \
 -I$(RGX_TOP)/services/system/rgx_mtk

ifeq ($(MTK_DDK_DEBUG),y)
ccflags-y += \
 -I$(RGX_TOP)/generated/ri_bridge \
 -I$(RGX_TOP)/generated/devicememhistory_bridge

endif


obj-y += pvrsrvkm.o
