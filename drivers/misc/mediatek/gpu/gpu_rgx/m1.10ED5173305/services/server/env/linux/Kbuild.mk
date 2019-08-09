########################################################################### ###
#@File
#@Copyright     Copyright (c) Imagination Technologies Ltd. All Rights Reserved
#@License       Dual MIT/GPLv2
# 
# The contents of this file are subject to the MIT license as set out below.
# 
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
# 
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
# 
# Alternatively, the contents of this file may be used under the terms of
# the GNU General Public License Version 2 ("GPL") in which case the provisions
# of GPL are applicable instead of those above.
# 
# If you wish to allow use of your version of this file only under the terms of
# GPL, and not to allow others to use your version of this file under the terms
# of the MIT license, indicate your decision by deleting the provisions above
# and replace them with the notice and other provisions required by GPL as set
# out in the file called "GPL-COPYING" included in this distribution. If you do
# not delete the provisions above, a recipient may use your version of this file
# under the terms of either the MIT license or GPL.
# 
# This License is also included in this distribution in the file called
# "MIT-COPYING".
# 
# EXCEPT AS OTHERWISE STATED IN A NEGOTIATED AGREEMENT: (A) THE SOFTWARE IS
# PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
# BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
# PURPOSE AND NONINFRINGEMENT; AND (B) IN NO EVENT SHALL THE AUTHORS OR
# COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
# IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
# CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
### ###########################################################################

# Compatibility BVNC
ccflags-y += -I$(TOP)/services/shared/devices/rgx

# Errata files
ccflags-y += -I$(TOP)/hwdefs

# Linux-specific headers
ccflags-y += \
 -I$(TOP)/services/include/env/linux \
 -I$(TOP)/kernel/drivers/staging/imgtec

$(PVRSRV_MODNAME)-y += \
 services/server/env/linux/event.o \
 services/server/env/linux/km_apphint.o \
 services/server/env/linux/module_common.o \
 services/server/env/linux/osmmap_stub.o \
 services/server/env/linux/osfunc.o \
 services/server/env/linux/allocmem.o \
 services/server/env/linux/osconnection_server.o \
 services/server/env/linux/pdump.o \
 services/server/env/linux/physmem_osmem_linux.o \
 services/server/env/linux/pmr_os.o \
 services/server/env/linux/pvr_debugfs.o \
 services/server/env/linux/pvr_bridge_k.o \
 services/server/env/linux/pvr_debug.o \
 services/server/env/linux/physmem_dmabuf.o \
 services/server/common/devicemem_heapcfg.o \
 services/shared/common/devicemem.o \
 services/shared/common/devicemem_utils.o \
 services/shared/common/hash.o \
 services/shared/common/ra.o \
 services/shared/common/sync.o \
 services/shared/common/mem_utils.o \
 services/server/common/devicemem_server.o \
 services/server/common/handle.o \
 services/server/common/lists.o \
 services/server/common/mmu_common.o \
 services/server/common/connection_server.o \
 services/server/common/physheap.o \
 services/server/common/physmem.o \
 services/server/common/physmem_lma.o \
 services/server/common/physmem_hostmem.o \
 services/server/common/physmem_tdsecbuf.o \
 services/server/common/pmr.o \
 services/server/common/power.o \
 services/server/common/process_stats.o \
 services/server/common/pvr_notifier.o \
 services/server/common/pvrsrv.o \
 services/server/common/srvcore.o \
 services/server/common/sync_checkpoint.o \
 services/server/common/sync_server.o \
 services/shared/common/htbuffer.o \
 services/server/common/htbserver.o \
 services/server/env/linux/htb_debug.o \
 services/server/common/tlintern.o \
 services/shared/common/tlclient.o \
 services/server/common/tlserver.o \
 services/server/common/tlstream.o \
 services/server/common/cache_km.o \
 services/shared/common/uniq_key_splay_tree.o \
 services/server/common/pvrsrv_pool.o \
 services/server/common/pvrsrv_bridge_init.o \
 services/server/common/info_page_km.o

# Wrap ExtMem support
ifeq ($(SUPPORT_WRAP_EXTMEM),1)
 $(PVRSRV_MODNAME)-y += \
 services/server/env/linux/physmem_extmem_linux.o \
 services/server/common/physmem_extmem.o 
endif

ifeq ($(SUPPORT_TRUSTED_DEVICE),1)
 $(PVRSRV_MODNAME)-y += \
 services/server/common/physmem_tdfwcode.o
endif

ifeq ($(SUPPORT_PHYSMEM_TEST),1)
 $(PVRSRV_MODNAME)-y += \
 services/server/env/linux/physmem_test.o
endif

ifeq ($(SUPPORT_DRM_EXT),)
 ifneq ($(PVR_LOADER),)
  ifeq ($(KERNEL_DRIVER_DIR),)
   $(PVRSRV_MODNAME)-y += kernel/drivers/staging/imgtec/$(PVR_LOADER).o
  else
   ifneq ($(wildcard $(KERNELDIR)/$(KERNEL_DRIVER_DIR)/$(PVR_SYSTEM)/$(PVR_LOADER).c),)
     $(PVRSRV_MODNAME)-y += external/$(KERNEL_DRIVER_DIR)/$(PVR_SYSTEM)/$(PVR_LOADER).o
   else
    ifneq ($(wildcard $(KERNELDIR)/$(KERNEL_DRIVER_DIR)/$(PVR_LOADER).c),)
      $(PVRSRV_MODNAME)-y += external/$(KERNEL_DRIVER_DIR)/$(PVR_LOADER).o
    else
      $(PVRSRV_MODNAME)-y += kernel/drivers/staging/imgtec/$(PVR_LOADER).o
    endif
   endif
  endif
 else
  $(PVRSRV_MODNAME)-y += kernel/drivers/staging/imgtec/pvr_platform_drv.o
 endif
endif


ifeq ($(SUPPORT_RGX),1)
$(PVRSRV_MODNAME)-y += \
 services/server/devices/rgx/debugmisc_server.o \
 services/server/devices/rgx/rgxbreakpoint.o \
 services/server/devices/rgx/rgxccb.o \
 services/server/devices/rgx/rgxdebug.o \
 services/server/devices/rgx/rgxfwutils.o \
 services/server/devices/rgx/rgxinit.o \
 services/server/devices/rgx/rgxbvnc.o \
 services/server/devices/rgx/rgxkicksync.o \
 services/server/devices/rgx/rgxlayer_impl.o \
 services/server/devices/rgx/rgxmem.o \
 services/server/devices/rgx/rgxmmuinit.o \
 services/server/devices/rgx/rgxregconfig.o \
 services/server/devices/rgx/rgxta3d.o \
 services/server/devices/rgx/rgxsyncutils.o \
 services/server/devices/rgx/rgxtimerquery.o \
 services/server/devices/rgx/rgxtransfer.o \
 services/server/devices/rgx/rgxtdmtransfer.o \
 services/server/devices/rgx/rgxutils.o \
 services/shared/devices/rgx/rgx_compat_bvnc.o \
 services/server/devices/rgx/rgxmipsmmuinit.o \
 services/server/devices/rgx/rgxhwperf.o \
 services/server/devices/rgx/rgxpower.o \
 services/server/devices/rgx/rgxstartstop.o \
 services/server/devices/rgx/rgxtimecorr.o \
 services/server/devices/rgx/rgxcompute.o \
 services/server/devices/rgx/rgxray.o \
 services/server/devices/rgx/rgxsignals.o
 
ifeq ($(SUPPORT_PDVFS),1)
 $(PVRSRV_MODNAME)-y += \
 services/server/devices/rgx/rgxpdvfs.o

 ifeq ($(SUPPORT_WORKLOAD_ESTIMATION),1)
  $(PVRSRV_MODNAME)-y += \
  services/server/devices/rgx/rgxworkest.o
 endif
endif
 
endif

ifeq ($(SUPPORT_DISPLAY_CLASS),1)
$(PVRSRV_MODNAME)-y += \
 services/server/common/dc_server.o \
 services/server/common/scp.o
endif

ifeq ($(SUPPORT_SECURE_EXPORT),1)
$(PVRSRV_MODNAME)-y += services/server/env/linux/ossecure_export.o
endif

ifeq ($(PDUMP),1)
$(PVRSRV_MODNAME)-y += \
 services/server/common/pdump_common.o \
 services/server/common/pdump_mmu.o \
 services/server/common/pdump_physmem.o \
 services/shared/common/devicemem_pdump.o \
 services/shared/common/devicememx_pdump.o
 
ifeq ($(SUPPORT_RGX),1)
$(PVRSRV_MODNAME)-y += \
 services/server/devices/rgx/rgxpdump.o
endif

endif



ifeq ($(PVR_RI_DEBUG),1)
$(PVRSRV_MODNAME)-y += services/server/common/ri_server.o
endif

ifeq ($(PVR_TESTING_UTILS),1)
$(PVRSRV_MODNAME)-y += services/server/common/tutils.o
endif

ifeq ($(SUPPORT_PAGE_FAULT_DEBUG),1)
$(PVRSRV_MODNAME)-y += services/server/common/devicemem_history_server.o
endif

ifeq ($(PVR_HANDLE_BACKEND),generic)
$(PVRSRV_MODNAME)-y += services/server/common/handle_generic.o
else
ifeq ($(PVR_HANDLE_BACKEND),idr)
$(PVRSRV_MODNAME)-y += services/server/env/linux/handle_idr.o
endif
endif

ifeq ($(SUPPORT_GPUTRACE_EVENTS),1)
$(PVRSRV_MODNAME)-y += services/server/env/linux/pvr_gputrace.o
endif

ifeq ($(PVRSRV_ENABLE_LINUX_MMAP_STATS),1)
$(PVRSRV_MODNAME)-y += services/server/env/linux/mmap_stats.o
endif

ifeq ($(SUPPORT_BUFFER_SYNC),1)
$(PVRSRV_MODNAME)-y += \
 kernel/drivers/staging/imgtec/pvr_buffer_sync.o \
 kernel/drivers/staging/imgtec/pvr_fence.o
endif

ifeq ($(SUPPORT_NATIVE_FENCE_SYNC),1)
ifeq ($(SUPPORT_DMA_FENCE),1)
$(PVRSRV_MODNAME)-y += \
 kernel/drivers/staging/imgtec/pvr_sync_file.o \
 kernel/drivers/staging/imgtec/pvr_counting_timeline.o \
 kernel/drivers/staging/imgtec/pvr_sw_fence.o \
 kernel/drivers/staging/imgtec/pvr_fence.o \
 services/server/env/linux/dma_fence_sync_native_server.o
else
$(PVRSRV_MODNAME)-y += services/server/env/linux/sync_native_server.o
$(PVRSRV_MODNAME)-y += kernel/drivers/staging/imgtec/pvr_sync2.o
endif
else
ifeq ($(SUPPORT_FALLBACK_FENCE_SYNC),1)
$(PVRSRV_MODNAME)-y += \
 services/server/common/sync_fallback_server.o \
 services/server/env/linux/ossecure_export.o
endif
endif

ifeq ($(PVR_DVFS),1)
$(PVRSRV_MODNAME)-y += \
 services/server/env/linux/pvr_dvfs_device.o
endif

$(PVRSRV_MODNAME)-$(CONFIG_X86) += services/server/env/linux/osfunc_x86.o
$(PVRSRV_MODNAME)-$(CONFIG_ARM) += services/server/env/linux/osfunc_arm.o
$(PVRSRV_MODNAME)-$(CONFIG_ARM64) += services/server/env/linux/osfunc_arm64.o
$(PVRSRV_MODNAME)-$(CONFIG_METAG) += services/server/env/linux/osfunc_metag.o
$(PVRSRV_MODNAME)-$(CONFIG_MIPS) += services/server/env/linux/osfunc_mips.o

$(PVRSRV_MODNAME)-$(CONFIG_EVENT_TRACING) += services/server/env/linux/trace_events.o

ifneq ($(SUPPORT_DRM_EXT),1)
ccflags-y += \
 -Iinclude/drm \
 -I$(TOP)/include/drm \
 -I$(TOP)/services/include/env/linux

$(PVRSRV_MODNAME)-y += \
 kernel/drivers/staging/imgtec/pvr_drm.o
endif # SUPPORT_DRM_EXT

ccflags-y += -I$(OUT)/target_neutral/intermediates/firmware

ifeq ($(SUPPORT_RGX),1)
# Srvinit headers and source files

$(PVRSRV_MODNAME)-y += \
 services/server/devices/rgx/rgxsrvinit.o \
 services/server/devices/rgx/rgxfwimageutils.o \
 services/shared/devices/rgx/rgx_compat_bvnc.o \
 services/shared/devices/rgx/rgx_hwperf_table.o \
 services/server/devices/rgx/env/linux/km/rgxfwload.o
endif

ccflags-y += \
 -Iinclude \
 -Ihwdefs \
 -Ihwdefs/km \
 -Iservices/include \
 -Iservices/include/shared \
 -Iservices/server/include \
 -Iservices/server/devices/rgx \
 -Iservices/shared/include \
 -Iservices/shared/devices/rgx

# Bridge headers and source files

# Keep in sync with:
# build/linux/common/bridges.mk AND
# services/bridge/Linux.mk

ccflags-y += \
 -I$(bridge_base)/mm_bridge \
 -I$(bridge_base)/cmm_bridge \
 -I$(bridge_base)/srvcore_bridge \
 -I$(bridge_base)/sync_bridge \
 -I$(bridge_base)/synctracking_bridge \
 -I$(bridge_base)/htbuffer_bridge \
 -I$(bridge_base)/pvrtl_bridge \
 -I$(bridge_base)/cache_bridge \
 -I$(bridge_base)/dmabuf_bridge

ifeq ($(SUPPORT_RGX),1)
ccflags-y += \
 -I$(bridge_base)/rgxtq_bridge \
 -I$(bridge_base)/rgxtq2_bridge \
 -I$(bridge_base)/rgxta3d_bridge \
 -I$(bridge_base)/rgxhwperf_bridge \
 -I$(bridge_base)/rgxkicksync_bridge \
 -I$(bridge_base)/rgxcmp_bridge \
 -I$(bridge_base)/rgxray_bridge \
 -I$(bridge_base)/breakpoint_bridge \
 -I$(bridge_base)/regconfig_bridge \
 -I$(bridge_base)/timerquery_bridge \
 -I$(bridge_base)/debugmisc_bridge \
 -I$(bridge_base)/rgxsignals_bridge
endif

$(PVRSRV_MODNAME)-y += \
 generated/mm_bridge/server_mm_bridge.o \
 generated/cmm_bridge/server_cmm_bridge.o \
 generated/srvcore_bridge/server_srvcore_bridge.o \
 generated/sync_bridge/server_sync_bridge.o \
 generated/htbuffer_bridge/server_htbuffer_bridge.o \
 generated/pvrtl_bridge/server_pvrtl_bridge.o \
 generated/cache_bridge/server_cache_bridge.o \
 generated/dmabuf_bridge/server_dmabuf_bridge.o

ifeq ($(SUPPORT_RGX),1)
$(PVRSRV_MODNAME)-y += \
 generated/rgxtq_bridge/server_rgxtq_bridge.o \
 generated/rgxtq2_bridge/server_rgxtq2_bridge.o \
 generated/rgxta3d_bridge/server_rgxta3d_bridge.o \
 generated/rgxhwperf_bridge/server_rgxhwperf_bridge.o \
 generated/rgxkicksync_bridge/server_rgxkicksync_bridge.o \
 generated/rgxcmp_bridge/server_rgxcmp_bridge.o \
 generated/rgxray_bridge/server_rgxray_bridge.o \
 generated/breakpoint_bridge/server_breakpoint_bridge.o \
 generated/regconfig_bridge/server_regconfig_bridge.o \
 generated/timerquery_bridge/server_timerquery_bridge.o \
 generated/debugmisc_bridge/server_debugmisc_bridge.o \
 generated/rgxsignals_bridge/server_rgxsignals_bridge.o
endif
  
ifeq ($(SUPPORT_WRAP_EXTMEM),1)
ccflags-y += -I$(bridge_base)/mmextmem_bridge
$(PVRSRV_MODNAME)-y += generated/mmextmem_bridge/server_mmextmem_bridge.o 
endif

ifeq ($(SUPPORT_DISPLAY_CLASS),1)
ccflags-y += -I$(bridge_base)/dc_bridge
$(PVRSRV_MODNAME)-y += generated/dc_bridge/server_dc_bridge.o
endif

ifeq ($(SUPPORT_SECURE_EXPORT),1)
ccflags-y += -I$(bridge_base)/smm_bridge
$(PVRSRV_MODNAME)-y += generated/smm_bridge/server_smm_bridge.o
endif

ifeq ($(SUPPORT_SERVER_SYNC),1)
ifeq ($(SUPPORT_SECURE_EXPORT),1)
ccflags-y += -I$(bridge_base)/syncsexport_bridge
$(PVRSRV_MODNAME)-y += generated/syncsexport_bridge/server_syncsexport_bridge.o
endif
ifeq ($(SUPPORT_INSECURE_EXPORT),1)
ccflags-y += \
 -I$(bridge_base)/syncexport_bridge
$(PVRSRV_MODNAME)-y += generated/syncexport_bridge/server_syncexport_bridge.o
endif
endif

ifeq ($(PDUMP),1)
ccflags-y += \
 -I$(bridge_base)/pdump_bridge \
 -I$(bridge_base)/pdumpctrl_bridge \
 -I$(bridge_base)/pdumpmm_bridge

ifeq ($(SUPPORT_RGX),1)
ccflags-y += \
 -I$(bridge_base)/rgxpdump_bridge 

$(PVRSRV_MODNAME)-y += \
 generated/rgxpdump_bridge/server_rgxpdump_bridge.o
endif
 
$(PVRSRV_MODNAME)-y += \
 generated/pdump_bridge/server_pdump_bridge.o \
 generated/pdumpctrl_bridge/server_pdumpctrl_bridge.o \
 generated/pdumpmm_bridge/server_pdumpmm_bridge.o
endif

ifeq ($(PVR_RI_DEBUG),1)
ccflags-y += -I$(bridge_base)/ri_bridge
$(PVRSRV_MODNAME)-y += generated/ri_bridge/server_ri_bridge.o
endif

ifeq ($(SUPPORT_VALIDATION),1)
ccflags-y += -I$(bridge_base)/validation_bridge
$(PVRSRV_MODNAME)-y += generated/validation_bridge/server_validation_bridge.o
$(PVRSRV_MODNAME)-y += services/server/common/validation.o
endif

ifeq ($(PVR_TESTING_UTILS),1)
ccflags-y += -I$(bridge_base)/tutils_bridge
$(PVRSRV_MODNAME)-y += generated/tutils_bridge/server_tutils_bridge.o
endif

ifeq ($(SUPPORT_PAGE_FAULT_DEBUG),1)
ccflags-y += -I$(bridge_base)/devicememhistory_bridge
$(PVRSRV_MODNAME)-y += \
 generated/devicememhistory_bridge/server_devicememhistory_bridge.o
endif

ifeq ($(SUPPORT_SYNCTRACKING_BRIDGE),1)
ccflags-y += -I$(bridge_base)/synctracking_bridge
$(PVRSRV_MODNAME)-y += \
 generated/synctracking_bridge/server_synctracking_bridge.o
endif

#ifeq ($(SUPPORT_SIGNAL_FILTER),1)
#endif

ifeq ($(SUPPORT_FALLBACK_FENCE_SYNC),1)
ccflags-y += \
 -I$(bridge_base)/syncfallback_bridge
$(PVRSRV_MODNAME)-y += generated/syncfallback_bridge/server_syncfallback_bridge.o
endif




# Direct bridges

$(PVRSRV_MODNAME)-y += \
 generated/mm_bridge/client_mm_direct_bridge.o \
 generated/sync_bridge/client_sync_direct_bridge.o \
 generated/htbuffer_bridge/client_htbuffer_direct_bridge.o \
 generated/cache_bridge/client_cache_direct_bridge.o \
 generated/pvrtl_bridge/client_pvrtl_direct_bridge.o

ifeq ($(PDUMP),1)
$(PVRSRV_MODNAME)-y += generated/pdumpmm_bridge/client_pdumpmm_direct_bridge.o
endif

ifeq ($(PVR_RI_DEBUG),1)
$(PVRSRV_MODNAME)-y += generated/ri_bridge/client_ri_direct_bridge.o
endif

ifeq ($(PDUMP),1)
 $(PVRSRV_MODNAME)-y += \
  generated/pdump_bridge/client_pdump_direct_bridge.o \
  generated/pdumpctrl_bridge/client_pdumpctrl_direct_bridge.o 
  
ifeq ($(SUPPORT_RGX),1)
 $(PVRSRV_MODNAME)-y += \
  generated/rgxpdump_bridge/client_rgxpdump_direct_bridge.o
endif

endif

ifeq ($(SUPPORT_PAGE_FAULT_DEBUG),1)
$(PVRSRV_MODNAME)-y += \
 generated/devicememhistory_bridge/client_devicememhistory_direct_bridge.o
endif

ifeq ($(SUPPORT_SYNCTRACKING_BRIDGE),1)
$(PVRSRV_MODNAME)-y += \
 generated/synctracking_bridge/client_synctracking_direct_bridge.o
endif

# Enable -Werror for all built object files (suppress for Fiasco.OC/L4Linux)
ifeq ($(CONFIG_L4),)
ifneq ($(W),1)
$(foreach _o,$(addprefix CFLAGS_,$(notdir $($(PVRSRV_MODNAME)-y))),$(eval $(_o) := -Werror))
endif
endif

# With certain build configurations, e.g., ARM, Werror, we get a build 
# failure in the ftrace Linux kernel header.  So disable the relevant check.
CFLAGS_trace_events.o := -Wno-missing-prototypes

# Make sure the mem_utils are built in 'free standing' mode, so the compiler
# is not encouraged to call out to C library functions
CFLAGS_mem_utils.o := -ffreestanding

# Chrome OS kernel adds some issues
ccflags-y += -Wno-ignored-qualifiers

include $(TOP)/services/system/$(PVR_SYSTEM)/Kbuild.mk
