OPTIM = -Os

$(info [GPU_DEBUG] MTK_PLATFORM = $(MTK_PLATFORM))

ifeq ($(MTK_PLATFORM),mt6765)
	ccflags-y += -DCONFIG_MACH_MT6765
else ifeq ($(MTK_PLATFORM),mt6779)
	ccflags-y += -DCONFIG_MACH_MT6779
else ifeq ($(MTK_PLATFORM),mt6761)
	ccflags-y += -DCONFIG_MACH_MT6761
else ifeq ($(MTK_PLATFORM),mt6739)
	ccflags-y += -DCONFIG_MACH_MT6739
endif

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
	generated/rogue/cache_bridge/server_cache_bridge.o \
	generated/rogue/cache_bridge/client_cache_direct_bridge.o \
	generated/rogue/rgxta3d_bridge/server_rgxta3d_bridge.o \
	generated/rogue/sync_bridge/client_sync_direct_bridge.o \
	generated/rogue/sync_bridge/server_sync_bridge.o \
	generated/rogue/synctracking_bridge/client_synctracking_direct_bridge.o \
	generated/rogue/synctracking_bridge/server_synctracking_bridge.o \
	generated/rogue/srvcore_bridge/server_srvcore_bridge.o \
	generated/rogue/devicememhistory_bridge/client_devicememhistory_direct_bridge.o \
	generated/rogue/devicememhistory_bridge/server_devicememhistory_bridge.o \
	generated/rogue/rgxcmp_bridge/server_rgxcmp_bridge.o \
	generated/rogue/rgxhwperf_bridge/server_rgxhwperf_bridge.o \
	generated/rogue/rgxfwdbg_bridge/server_rgxfwdbg_bridge.o \
	generated/rogue/rgxbreakpoint_bridge/server_rgxbreakpoint_bridge.o \
	generated/rogue/rgxregconfig_bridge/server_rgxregconfig_bridge.o \
	generated/rogue/htbuffer_bridge/client_htbuffer_direct_bridge.o \
	generated/rogue/htbuffer_bridge/server_htbuffer_bridge.o \
	generated/rogue/cmm_bridge/server_cmm_bridge.o \
	generated/rogue/rgxtq_bridge/server_rgxtq_bridge.o \
	generated/rogue/mm_bridge/client_mm_direct_bridge.o \
	generated/rogue/mm_bridge/server_mm_bridge.o \
	generated/rogue/rgxkicksync_bridge/server_rgxkicksync_bridge.o \
	generated/rogue/pvrtl_bridge/server_pvrtl_bridge.o \
	generated/rogue/pvrtl_bridge/client_pvrtl_direct_bridge.o \
	generated/rogue/rgxsignals_bridge/server_rgxsignals_bridge.o \
	generated/rogue/rgxtq2_bridge/server_rgxtq2_bridge.o \
	generated/rogue/dmabuf_bridge/server_dmabuf_bridge.o \
	services/system/rogue/rgx_mtk/ion_support.o \
	services/system/rogue/rgx_mtk/sysconfig.o \
	services/system/rogue/rgx_mtk/$(MTK_PLATFORM)/mtk_mfgsys.o \
	services/system/rogue/rgx_mtk/mtk_pp.o \
	services/system/rogue/common/vmm_type_stub.o \
	services/system/rogue/common/env/linux/interrupt_support.o \
	services/system/rogue/common/env/linux/dma_support.o \
	services/server/devices/rgxtimecorr.o \
	services/server/devices/rgxfwdbg.o \
	services/server/devices/rgxshader.o \
	services/server/devices/rgxfwtrace_strings.o \
	services/server/devices/rogue/rgxutils.o \
	services/server/devices/rogue/rgxhwperf.o \
	services/server/devices/rogue/rgxfwimageutils.o \
	services/server/devices/rogue/rgxfwutils.o \
	services/server/devices/rogue/rgxmem.o \
	services/server/devices/rogue/rgxccb.o \
	services/server/devices/rogue/rgxmmuinit.o \
	services/server/devices/rogue/rgxtransfer.o \
	services/server/devices/rogue/rgxbreakpoint.o \
	services/server/devices/rogue/rgxmipsmmuinit.o \
	services/server/devices/rogue/rgxinit.o \
	services/server/devices/rogue/rgxsignals.o \
	services/server/devices/rogue/rgxlayer_impl.o \
	services/server/devices/rogue/rgxtdmtransfer.o \
	services/server/devices/rogue/rgxbvnc.o \
	services/server/devices/rogue/rgxdebug.o \
	services/server/devices/rogue/rgxta3d.o \
	services/server/devices/rogue/rgxkicksync.o \
	services/server/devices/rogue/rgxsrvinit.o \
	services/server/devices/rogue/rgxregconfig.o \
	services/server/devices/rogue/rgxcompute.o \
	services/server/devices/rogue/rgxstartstop.o \
	services/server/devices/rogue/rgxpower.o \
	services/server/devices/rogue/rgxsyncutils.o \
	services/server/devices/rogue/rgxmulticore.o \
	services/server/common/tlstream.o \
	services/server/common/pvrsrv_pool.o \
	services/server/common/cache_km.o \
	services/server/common/htbserver.o \
	services/server/common/tlintern.o \
	services/server/common/pvr_notifier.o \
	services/server/common/devicemem_history_server.o \
	services/server/common/physmem.o \
	services/server/common/tlserver.o \
	services/server/common/physheap.o \
	services/server/common/handle.o \
	services/server/common/devicemem_heapcfg.o \
	services/server/common/sync_server.o \
	services/server/common/sync_checkpoint.o \
	services/server/common/lists.o \
	services/server/common/info_page_km.o \
	services/server/common/physmem_lma.o \
	services/server/common/process_stats.o \
	services/server/common/connection_server.o \
	services/server/common/pmr.o \
	services/server/common/pvrsrv_bridge_init.o \
	services/server/common/mmu_common.o \
	services/server/common/devicemem_server.o \
	services/server/common/power.o \
	services/server/common/physmem_hostmem.o \
	services/server/common/pvrsrv.o \
	services/server/common/srvcore.o \
	services/server/common/vz_vmm_pvz.o \
	services/server/common/vz_vmm_vm.o \
	services/server/common/vmm_pvz_client.o \
	services/server/common/vmm_pvz_server.o \
	services/server/common/htb_debug.o \
	services/server/common/di_server.o \
	services/server/env/linux/km_apphint.o \
	services/server/env/linux/osmmap_stub.o \
	services/server/env/linux/physmem_test.o \
	services/server/env/linux/allocmem.o \
	services/server/env/linux/pvr_bridge_k.o \
	services/server/env/linux/physmem_dmabuf.o \
	services/server/env/linux/pvr_debugfs.o \
	services/server/env/linux/osconnection_server.o \
	services/server/env/linux/handle_idr.o \
	services/server/env/linux/event.o \
	services/server/env/linux/fwload.o \
	services/server/env/linux/pmr_os.o \
	services/server/env/linux/module_common.o \
	services/server/env/linux/physmem_osmem_linux.o \
	services/server/env/linux/osfunc.o \
	services/server/env/linux/pvr_gputrace.o \
	services/server/env/linux/pvr_debug.o \
	services/server/env/linux/pvr_procfs.o \
	services/server/env/linux/pvr_ion_stats.o \
	services/shared/devices/rogue/rgx_hwperf_table.o \
	services/shared/common/devicemem_utils.o \
	services/shared/common/hash.o \
	services/shared/common/ra.o \
	services/shared/common/sync.o \
	services/shared/common/uniq_key_splay_tree.o \
	services/shared/common/tlclient.o \
	services/shared/common/devicemem.o \
	services/shared/common/htbuffer.o \
	services/shared/common/mem_utils.o \
	services/shared/common/pvrsrv_error.o \
	kernel/drivers/staging/imgtec/pvr_drm.o \
	kernel/drivers/staging/imgtec/pvr_platform_drv.o \

ifeq ($(CONFIG_SYNC_FILE),y)
	pvrsrvkm-y += \
		kernel/drivers/staging/imgtec/pvr_sync_file.o \
		kernel/drivers/staging/imgtec/pvr_counting_timeline.o \
		kernel/drivers/staging/imgtec/pvr_sw_fence.o \
		kernel/drivers/staging/imgtec/pvr_fence.o
else
	pvrsrvkm-y += \
		kernel/drivers/staging/imgtec/pvr_sync2.o
endif

pvrsrvkm-$(CONFIG_ARM) += services/server/env/linux/osfunc_arm.o
pvrsrvkm-$(CONFIG_ARM64) += services/server/env/linux/osfunc_arm64.o
pvrsrvkm-$(CONFIG_EVENT_TRACING) += services/server/env/linux/trace_events.o

ifeq ($(MTK_PLATFORM),mt6799)
	pvrsrvkm-y += \
		services/system/rogue/rgx_mtk/$(MTK_PLATFORM)/mtk_mfg_counter.o
endif

ccflags-y += \
	-I$(RGX_TOP)/hwdefs/rogue \
	-I$(RGX_TOP)/hwdefs/rogue/km \
	-I$(RGX_TOP)/hwdefs/rogue/km/configs \
	-I$(RGX_TOP)/hwdefs/rogue/km/cores \
	-I$(RGX_TOP)/include \
	-I$(RGX_TOP)/include/rogue \
	-I$(RGX_TOP)/include/drm \
	-I$(RGX_TOP)/include/public \
	-I$(RGX_TOP)/kernel/drivers/staging/imgtec \
	-I$(RGX_TOP)/generated/rogue/cache_bridge \
	-I$(RGX_TOP)/generated/rogue/cmm_bridge \
	-I$(RGX_TOP)/generated/rogue/dmabuf_bridge \
	-I$(RGX_TOP)/generated/rogue/htbuffer_bridge \
	-I$(RGX_TOP)/generated/rogue/mm_bridge \
	-I$(RGX_TOP)/generated/rogue/pvrtl_bridge \
	-I$(RGX_TOP)/generated/rogue/rgxbreakpoint_bridge \
	-I$(RGX_TOP)/generated/rogue/rgxcmp_bridge \
	-I$(RGX_TOP)/generated/rogue/rgxfwdbg_bridge \
	-I$(RGX_TOP)/generated/rogue/rgxhwperf_bridge \
	-I$(RGX_TOP)/generated/rogue/rgxinit_bridge \
	-I$(RGX_TOP)/generated/rogue/rgxkicksync_bridge \
	-I$(RGX_TOP)/generated/rogue/rgxregconfig_bridge \
	-I$(RGX_TOP)/generated/rogue/rgxta3d_bridge \
	-I$(RGX_TOP)/generated/rogue/rgxtq_bridge \
	-I$(RGX_TOP)/generated/rogue/rgxtq2_bridge \
	-I$(RGX_TOP)/generated/rogue/srvcore_bridge \
	-I$(RGX_TOP)/generated/rogue/sync_bridge \
	-I$(RGX_TOP)/generated/rogue/synctracking_bridge \
	-I$(RGX_TOP)/generated/rogue/devicememhistory_bridge \
	-I$(RGX_TOP)/generated/rogue/rgxsignals_bridge \
	-I$(RGX_TOP)/generated/rogue/ri_bridge \
	-I$(RGX_TOP)/services/include \
	-I$(RGX_TOP)/services/include/rogue \
	-I$(RGX_TOP)/services/include/env/linux \
	-I$(RGX_TOP)/services/server/devices \
	-I$(RGX_TOP)/services/server/devices/rogue \
	-I$(RGX_TOP)/services/server/env/linux \
	-I$(RGX_TOP)/services/server/env/linux/rogue \
	-I$(RGX_TOP)/services/server/include \
	-I$(RGX_TOP)/services/server/include/rogue \
	-I$(RGX_TOP)/services/shared/common \
	-I$(RGX_TOP)/services/shared/devices/rogue \
	-I$(RGX_TOP)/services/shared/include \
	-I$(RGX_TOP)/services/system/rogue/include \
	-I$(RGX_TOP)/services/system/rogue/rgx_mtk \
	-I$(RGX_TOP)/services/system/rogue/rgx_mtk/$(MTK_PLATFORM)

# GPU mt-plat: drivers/gpu/mediatek/mt-plat
# Legacy mt-plat: drivers/misc/mediatek/include/mt-plat
ccflags-y += \
	-I$(srctree)/drivers/staging/android/ion \
	-I$(srctree)/drivers/staging/android/ion/mtk \
	-I$(srctree)/drivers/gpu/mediatek \
	-I$(srctree)/drivers/gpu/mediatek/ged/include \
	-I$(srctree)/drivers/gpu/mediatek/mt-plat \
	-I$(srctree)/drivers/gpu/mediatek/gpufreq/include \
	-I$(srctree)/drivers/gpu/mediatek/gpu_bm \
	-I$(srctree)/drivers/misc/mediatek/include/mt-plat \

obj-$(CONFIG_MTK_GPU_SUPPORT) += pvrsrvkm.o
