#include $(srctree)/drivers/misc/mediatek/Makefile.custom

OPTIM = -Os

RGX_TOP := $(srctree)/drivers/misc/mediatek/gpu/$(MTK_PLATFORM)/1.5_ED3830101/

ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/$(MTK_PLATFORM)

VER := $(if $(filter eng,$(TARGET_BUILD_VARIANT)),eng,user)
ccflags-y += -include $(RGX_TOP)/include/config_kernel_$(VER).h

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
 -Wno-type-limits

pvrsrvkm-y += \
 generated/srvcore_bridge/server_srvcore_bridge.o \
 generated/dpvrtl_bridge/client_pvrtl_bridge.o \
 generated/debugmisc_bridge/server_debugmisc_bridge.o \
 generated/breakpoint_bridge/server_breakpoint_bridge.o \
 generated/mm_bridge/server_mm_bridge.o \
 generated/cmm_bridge/server_cmm_bridge.o \
 generated/regconfig_bridge/server_regconfig_bridge.o \
 generated/rgxhwperf_bridge/server_rgxhwperf_bridge.o \
 generated/pdumpctrl_bridge/server_pdumpctrl_bridge.o \
 generated/dpdumpmm_bridge/client_pdumpmm_bridge.o \
 generated/rgxta3d_bridge/server_rgxta3d_bridge.o \
 generated/syncsexport_bridge/server_syncsexport_bridge.o \
 generated/rgxinit_bridge/server_rgxinit_bridge.o \
 generated/timerquery_bridge/server_timerquery_bridge.o \
 generated/pvrtl_bridge/server_pvrtl_bridge.o \
 generated/pdumpmm_bridge/server_pdumpmm_bridge.o \
 generated/rgxtq_bridge/server_rgxtq_bridge.o \
 generated/smm_bridge/server_smm_bridge.o \
 generated/rgxcmp_bridge/server_rgxcmp_bridge.o \
 generated/dsync_bridge/client_sync_bridge.o \
 generated/pdump_bridge/server_pdump_bridge.o \
 generated/dmabuf_bridge/server_dmabuf_bridge.o \
 generated/rgxpdump_bridge/server_rgxpdump_bridge.o \
 generated/dmm_bridge/client_mm_bridge.o \
 generated/cachegeneric_bridge/server_cachegeneric_bridge.o \
 generated/sync_bridge/server_sync_bridge.o \
 generated/ddevicememhistory_bridge/client_devicememhistory_bridge.o \
 generated/devicememhistory_bridge/server_devicememhistory_bridge.o \
 services/system/rgx_mtk/ion_support.o \
 services/system/rgx_mtk/sysconfig.o \
 services/system/rgx_mtk/mtk_pp.o \
 services/system/rgx_mtk/mtk_mfgsys.o \
 services/shared/devices/rgx/rgx_compat_bvnc.o \
 services/shared/common/hash.o \
 services/shared/common/tlclient.o \
 services/shared/common/devicemem_utils.o \
 services/shared/common/dllist.o \
 services/shared/common/devicemem.o \
 services/shared/common/mem_utils.o \
 services/shared/common/sync.o \
 services/shared/common/ra.o \
 services/shared/common/uniq_key_splay_tree.o \
 services/server/devices/rgx/rgxsync.o \
 services/server/devices/rgx/rgxregconfig.o \
 services/server/devices/rgx/rgxtransfer.o \
 services/server/devices/rgx/rgxta3d.o \
 services/server/devices/rgx/rgxfwutils.o \
 services/server/devices/rgx/rgxinit.o \
 services/server/devices/rgx/rgxcompute.o \
 services/server/devices/rgx/rgxccb.o \
 services/server/devices/rgx/rgxhwperf.o \
 services/server/devices/rgx/rgxmmuinit.o \
 services/server/devices/rgx/rgxmem.o \
 services/server/devices/rgx/rgxpower.o \
 services/server/devices/rgx/debugmisc_server.o \
 services/server/devices/rgx/rgxbreakpoint.o \
 services/server/devices/rgx/rgxtimerquery.o \
 services/server/devices/rgx/rgxtimecorr.o \
 services/server/devices/rgx/rgxutils.o \
 services/server/devices/rgx/rgxdebug.o \
 services/server/common/tlintern.o \
 services/server/common/physmem.o \
 services/server/common/lists.o \
 services/server/common/srvcore.o \
 services/server/common/connection_server.o \
 services/server/common/process_stats.o \
 services/server/common/pmr.o \
 services/server/common/tlstream.o \
 services/server/common/devicemem_heapcfg.o \
 services/server/common/mmu_common.o \
 services/server/common/power.o \
 services/server/common/devicemem_server.o \
 services/server/common/devicemem_history_server.o \
 services/server/common/physheap.o \
 services/server/common/physmem_lma.o \
 services/server/common/cache_generic.o \
 services/server/common/tlserver.o \
 services/server/common/sync_server.o \
 services/server/common/handle.o \
 services/server/common/pvrsrv.o \
 services/server/env/linux/handle_idr.o \
 services/server/env/linux/mmap.o \
 services/server/env/linux/osfunc.o \
 services/server/env/linux/mm.o \
 services/server/env/linux/ossecure_export.o \
 services/server/env/linux/pdump.o \
 services/server/env/linux/physmem_dmabuf.o \
 services/server/env/linux/physmem_osmem_linux.o \
 services/server/env/linux/pvr_bridge_k.o \
 services/server/env/linux/module.o \
 services/server/env/linux/module_common.o \
 services/server/env/linux/osconnection_server.o \
 services/server/env/linux/physmem_tdsecbuf_linux.o \
 services/server/env/linux/pvr_debugfs.o \
 services/server/env/linux/devicemem_mmap_stub.o \
 services/server/env/linux/physmem_tdmetacode_linux.o \
 services/server/env/linux/event.o \
 services/server/env/linux/allocmem.o \
 services/server/env/linux/pvr_debug.o \
 services/server/env/linux/pvr_gputrace.o \
 kernel/drivers/staging/imgtec/pvr_sync.o

pvrsrvkm-$(CONFIG_ARM) += services/server/env/linux/osfunc_arm.o
pvrsrvkm-$(CONFIG_ARM64) += services/server/env/linux/osfunc_arm64.o
pvrsrvkm-$(CONFIG_EVENT_TRACING) += services/server/env/linux/trace_events.o

ccflags-y += \
 -I$(srctree)/drivers/staging/android/ion

ccflags-y += \
 -I$(RGX_TOP)/generated/breakpoint_bridge \
 -I$(RGX_TOP)/generated/cachegeneric_bridge \
 -I$(RGX_TOP)/generated/cmm_bridge \
 -I$(RGX_TOP)/generated/debugmisc_bridge \
 -I$(RGX_TOP)/generated/dmabuf_bridge \
 -I$(RGX_TOP)/generated/dmm_bridge \
 -I$(RGX_TOP)/generated/dsync_bridge \
 -I$(RGX_TOP)/generated/mm_bridge \
 -I$(RGX_TOP)/generated/pdump_bridge \
 -I$(RGX_TOP)/generated/pdumpctrl_bridge \
 -I$(RGX_TOP)/generated/pdumpmm_bridge \
 -I$(RGX_TOP)/generated/pvrtl_bridge \
 -I$(RGX_TOP)/generated/regconfig_bridge \
 -I$(RGX_TOP)/generated/rgxcmp_bridge \
 -I$(RGX_TOP)/generated/rgxhwperf_bridge \
 -I$(RGX_TOP)/generated/rgxinit_bridge \
 -I$(RGX_TOP)/generated/rgxpdump_bridge \
 -I$(RGX_TOP)/generated/rgxta3d_bridge \
 -I$(RGX_TOP)/generated/rgxtq_bridge \
 -I$(RGX_TOP)/generated/smm_bridge \
 -I$(RGX_TOP)/generated/srvcore_bridge \
 -I$(RGX_TOP)/generated/sync_bridge \
 -I$(RGX_TOP)/generated/syncsexport_bridge \
 -I$(RGX_TOP)/generated/timerquery_bridge \
 -I$(RGX_TOP)/generated/devicememhistory_bridge \
 -I$(RGX_TOP)/hwdefs \
 -I$(RGX_TOP)/hwdefs/km \
 -I$(RGX_TOP)/hwdefs/km/configs \
 -I$(RGX_TOP)/hwdefs/km/cores \
 -I$(RGX_TOP)/include \
 -I$(RGX_TOP)/services/include \
 -I$(RGX_TOP)/services/include/shared \
 -I$(RGX_TOP)/services/include/env/linux \
 -I$(RGX_TOP)/services/server/include \
 -I$(RGX_TOP)/services/server/devices/rgx \
 -I$(RGX_TOP)/services/server/env/linux \
 -I$(RGX_TOP)/services/shared/common \
 -I$(RGX_TOP)/services/shared/devices/rgx \
 -I$(RGX_TOP)/services/shared/include \
 -I$(RGX_TOP)/services/system/include \
 -I$(RGX_TOP)/services/system/rgx_mtk \
 -I$(RGX_TOP)/kernel/drivers/staging/imgtec

obj-y += pvrsrvkm.o
