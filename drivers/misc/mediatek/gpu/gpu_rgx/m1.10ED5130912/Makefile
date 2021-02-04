OPTIM = -Os

MTK_GPU_VERSION = $(subst ",,$(CONFIG_MTK_GPU_VERSION))

ccflags-$(CONFIG_MTK_TINYSYS_SSPM_SUPPORT) += -DSSPM_DEBUG

ifeq ($(word 2,$(MTK_GPU_VERSION)),marlowe)
	ECO := v2
	ccflags-y += -DMTCMOS_MFG3
	ccflags-y += -DMTK_MARLOWE
else
	ECO := ""
endif

RGX_TOP := $(dir $(lastword $(MAKEFILE_LIST)))

#MTK_DDK_DEBUG := y

ifeq ($(MTK_DDK_DEBUG),y)
	ccflags-y += -include $(RGX_TOP)/include/config_kernel_debug_$(MTK_PLATFORM)$(ECO).h
else ifeq ($(CONFIG_MTK_ENG_BUILD),y)
	ccflags-y += -include $(RGX_TOP)/include/config_kernel_eng_$(MTK_PLATFORM)$(ECO).h
else
	ccflags-y += -include $(RGX_TOP)/include/config_kernel_user_$(MTK_PLATFORM)$(ECO).h
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
	generated/rgxray_bridge/server_rgxray_bridge.o \
	generated/timerquery_bridge/server_timerquery_bridge.o \
	generated/debugmisc_bridge/server_debugmisc_bridge.o \
	generated/breakpoint_bridge/server_breakpoint_bridge.o \
	generated/cache_bridge/server_cache_bridge.o \
	generated/cache_bridge/client_cache_direct_bridge.o \
	generated/rgxta3d_bridge/server_rgxta3d_bridge.o \
	generated/sync_bridge/client_sync_direct_bridge.o \
	generated/sync_bridge/server_sync_bridge.o \
	generated/srvcore_bridge/server_srvcore_bridge.o \
	generated/devicememhistory_bridge/client_devicememhistory_direct_bridge.o \
	generated/devicememhistory_bridge/server_devicememhistory_bridge.o \
	generated/rgxcmp_bridge/server_rgxcmp_bridge.o \
	generated/rgxhwperf_bridge/server_rgxhwperf_bridge.o \
	generated/htbuffer_bridge/client_htbuffer_direct_bridge.o \
	generated/htbuffer_bridge/server_htbuffer_bridge.o \
	generated/cmm_bridge/server_cmm_bridge.o \
	generated/rgxtq_bridge/server_rgxtq_bridge.o \
	generated/mm_bridge/client_mm_direct_bridge.o \
	generated/mm_bridge/server_mm_bridge.o \
	generated/rgxkicksync_bridge/server_rgxkicksync_bridge.o \
	generated/pvrtl_bridge/server_pvrtl_bridge.o \
	generated/pvrtl_bridge/client_pvrtl_direct_bridge.o \
	generated/regconfig_bridge/server_regconfig_bridge.o \
	generated/rgxsignals_bridge/server_rgxsignals_bridge.o \
	generated/rgxtq2_bridge/server_rgxtq2_bridge.o \
	generated/dmabuf_bridge/server_dmabuf_bridge.o \
	services/system/rgx_mtk/ion_support.o \
	services/system/rgx_mtk/sysconfig.o \
	services/system/rgx_mtk/$(MTK_PLATFORM)/mtk_mfgsys.o \
	services/system/rgx_mtk/mtk_pp.o \
	services/system/common/vz_vmm_pvz.o \
	services/system/common/vz_vmm_vm.o \
	services/system/common/vz_physheap_common.o \
	services/system/common/vmm_type_stub.o \
	services/system/common/vmm_pvz_server.o \
	services/system/common/vz_support.o \
	services/system/common/vz_physheap_generic.o \
	services/system/common/vmm_pvz_client.o \
	services/system/common/env/linux/interrupt_support.o \
	services/system/common/env/linux/dma_support.o \
	services/server/devices/rgx/rgxutils.o \
	services/server/devices/rgx/rgxray.o \
	services/server/devices/rgx/rgxhwperf.o \
	services/server/devices/rgx/rgxfwimageutils.o \
	services/server/devices/rgx/rgxfwutils.o \
	services/server/devices/rgx/rgxmem.o \
	services/server/devices/rgx/rgxccb.o \
	services/server/devices/rgx/rgxmmuinit.o \
	services/server/devices/rgx/rgxtransfer.o \
	services/server/devices/rgx/rgxbreakpoint.o \
	services/server/devices/rgx/rgxmipsmmuinit.o \
	services/server/devices/rgx/rgxinit.o \
	services/server/devices/rgx/rgxsignals.o \
	services/server/devices/rgx/rgxlayer_impl.o \
	services/server/devices/rgx/rgxtimecorr.o \
	services/server/devices/rgx/rgxtdmtransfer.o \
	services/server/devices/rgx/rgxbvnc.o \
	services/server/devices/rgx/rgxdebug.o \
	services/server/devices/rgx/rgxta3d.o \
	services/server/devices/rgx/rgxkicksync.o \
	services/server/devices/rgx/rgxsrvinit.o \
	services/server/devices/rgx/rgxregconfig.o \
	services/server/devices/rgx/rgxcompute.o \
	services/server/devices/rgx/rgxstartstop.o \
	services/server/devices/rgx/rgxpower.o \
	services/server/devices/rgx/rgxsyncutils.o \
	services/server/devices/rgx/debugmisc_server.o \
	services/server/devices/rgx/env/linux/km/rgxfwload.o \
	services/server/devices/rgx/rgxtimerquery.o \
	services/server/common/power.o \
	services/server/common/physmem_tdsecbuf.o \
	services/server/common/info_page_km.o \
	services/server/common/devicemem_server.o \
	services/server/common/tlstream.o \
	services/server/common/physmem_hostmem.o \
	services/server/common/pvrsrv_pool.o \
	services/server/common/cache_km.o \
	services/server/common/htbserver.o \
	services/server/common/tlintern.o \
	services/server/common/mmu_common.o \
	services/server/common/pvr_notifier.o \
	services/server/common/pvrsrv_bridge_init.o \
	services/server/common/devicemem_history_server.o \
	services/server/common/physmem.o \
	services/server/common/tlserver.o \
	services/server/common/physheap.o \
	services/server/common/handle.o \
	services/server/common/srvcore.o \
	services/server/common/process_stats.o \
	services/server/common/connection_server.o \
	services/server/common/pmr.o \
	services/server/common/devicemem_heapcfg.o \
	services/server/common/sync_server.o \
	services/server/common/sync_checkpoint.o \
	services/server/common/lists.o \
	services/server/common/physmem_lma.o \
	services/server/common/pvrsrv.o \
	services/server/env/linux/km_apphint.o \
	services/server/env/linux/pdump.o \
	services/server/env/linux/pvr_gputrace.o \
	services/server/env/linux/event.o \
	services/server/env/linux/osmmap_stub.o \
	services/server/env/linux/trace_events.o \
	services/server/env/linux/handle_idr.o \
	services/server/env/linux/pvr_debug.o \
	services/server/env/linux/physmem_osmem_linux.o \
	services/server/env/linux/osfunc.o \
	services/server/env/linux/allocmem.o \
	services/server/env/linux/pvr_bridge_k.o \
	services/server/env/linux/physmem_dmabuf.o \
	services/server/env/linux/htb_debug.o \
	services/server/env/linux/pmr_os.o \
	services/server/env/linux/module_common.o \
	services/server/env/linux/pvr_debugfs.o \
	services/server/env/linux/osconnection_server.o \
	services/shared/devices/rgx/rgx_hwperf_table.o \
	services/shared/devices/rgx/rgx_compat_bvnc.o \
	services/shared/common/devicemem_utils.o \
	services/shared/common/hash.o \
	services/shared/common/ra.o \
	services/shared/common/sync.o \
	services/shared/common/uniq_key_splay_tree.o \
	services/shared/common/tlclient.o \
	services/shared/common/devicemem.o \
	services/shared/common/htbuffer.o \
	services/shared/common/mem_utils.o \
	kernel/drivers/staging/imgtec/pvr_drm.o \
	kernel/drivers/staging/imgtec/pvr_platform_drv.o \

ifeq ($(CONFIG_SYNC_FILE),y)
	pvrsrvkm-y += \
		kernel/drivers/staging/imgtec/pvr_sync_file.o \
		kernel/drivers/staging/imgtec/pvr_counting_timeline.o \
		kernel/drivers/staging/imgtec/pvr_sw_fence.o \
		kernel/drivers/staging/imgtec/pvr_fence.o \
		services/server/env/linux/dma_fence_sync_native_server.o
else
	pvrsrvkm-y += \
		services/server/env/linux/sync_native_server.o \
		kernel/drivers/staging/imgtec/pvr_sync2.o
endif

pvrsrvkm-$(CONFIG_ARM) += services/server/env/linux/osfunc_arm.o
pvrsrvkm-$(CONFIG_ARM64) += services/server/env/linux/osfunc_arm64.o
pvrsrvkm-$(CONFIG_EVENT_TRACING) += services/server/env/linux/trace_events.o

ifeq ($(MTK_PLATFORM),mt6799)
	pvrsrvkm-y += \
		services/system/rgx_mtk/$(MTK_PLATFORM)/mtk_mfg_counter.o
endif

ccflags-y += \
	-I$(RGX_TOP)/hwdefs \
	-I$(RGX_TOP)/hwdefs/km \
	-I$(RGX_TOP)/hwdefs/km/configs \
	-I$(RGX_TOP)/hwdefs/km/cores \
	-I$(RGX_TOP)/include \
	-I$(RGX_TOP)/include/drm \
	-I$(RGX_TOP)/include/public \
	-I$(RGX_TOP)/kernel/drivers/staging/imgtec \
	-I$(RGX_TOP)/generated/breakpoint_bridge \
	-I$(RGX_TOP)/generated/cache_bridge \
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
	-I$(RGX_TOP)/generated/rgxray_bridge \
	-I$(RGX_TOP)/generated/rgxta3d_bridge \
	-I$(RGX_TOP)/generated/rgxtq_bridge \
	-I$(RGX_TOP)/generated/rgxtq2_bridge \
	-I$(RGX_TOP)/generated/srvcore_bridge \
	-I$(RGX_TOP)/generated/sync_bridge \
	-I$(RGX_TOP)/generated/timerquery_bridge \
	-I$(RGX_TOP)/generated/devicememhistory_bridge \
	-I$(RGX_TOP)/generated/rgxsignals_bridge \
	-I$(RGX_TOP)/generated/ri_bridge \
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
	-I$(RGX_TOP)/services/system/rgx_mtk \
	-I$(RGX_TOP)/services/system/rgx_mtk/$(MTK_PLATFORM)

ccflags-y += \
	-I$(srctree)/drivers/staging/android/ion \
	-I$(srctree)/drivers/staging/android/ion/mtk \
	-I$(srctree)/drivers/misc/mediatek/gpu/ged/include \
	-I$(srctree)/drivers/misc/mediatek/include/mt-plat \
	-I$(srctree)/drivers/misc/mediatek/base/power/$(MTK_PLATFORM)

ifeq ($(MTK_PLATFORM),mt6799)
	ccflags-y += \
		-I$(srctree)/drivers/misc/mediatek/pmic/include/mt6335
endif

obj-y += pvrsrvkm.o
