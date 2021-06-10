########################################################################### ###
#@Title         Non-public feature checks
#@Copyright     Copyright (c) Imagination Technologies Ltd. All Rights Reserved
#@License       Strictly Confidential.
### ###########################################################################

# Put feature checks requiring NDA/partner source access here.
#
# Enable dumpstate hal.
#
PVR_ANDROID_HAS_DUMPSTATE_HAL_V1_1 ?= 1

# Support NV_context_priority_realtime EGL extension.
# On Android, SurfaceFlinger will elevate to using a REALTIME priority to
# eliminate jank in the UI.
#
EGL_EXTENSION_NV_CONTEXT_PRIORITY_REALTIME ?= 1

# Add libdmabufheap in gralloc to allow using a mix of ION and DMA-BUF heaps.
#
PVR_ANDROID_HAS_LIBDMABUFHEAP ?= 1
