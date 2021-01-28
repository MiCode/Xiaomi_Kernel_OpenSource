/*************************************************************************/ /*!
@File
@Title          PVR ION memory stats
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    Implementation of ION memory stats.
@License        Dual MIT/GPLv2

The contents of this file are subject to the MIT license as set out below.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

Alternatively, the contents of this file may be used under the terms of
the GNU General Public License Version 2 ("GPL") in which case the provisions
of GPL are applicable instead of those above.

If you wish to allow use of your version of this file only under the terms of
GPL, and not to allow others to use your version of this file under the terms
of the MIT license, indicate your decision by deleting the provisions above
and replace them with the notice and other provisions required by GPL as set
out in the file called "GPL-COPYING" included in this distribution. If you do
not delete the provisions above, a recipient may use your version of this file
under the terms of either the MIT license or GPL.

This License is also included in this distribution in the file called
"MIT-COPYING".

EXCEPT AS OTHERWISE STATED IN A NEGOTIATED AGREEMENT: (A) THE SOFTWARE IS
PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
PURPOSE AND NONINFRINGEMENT; AND (B) IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/ /**************************************************************************/

#include <linux/log2.h>
#include <linux/version.h>
#include <linux/rbtree.h>
#include <linux/dma-buf.h>
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 11, 0))
#include <linux/sched/task.h>
#else
#include <linux/sched.h>
#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 11, 0)) */
#include PVR_ANDROID_ION_HEADER
#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 12, 0))
#include PVR_ANDROID_ION_PRIV_HEADER
#endif /* (LINUX_VERSION_CODE < KERNEL_VERSION(4, 12, 0)) */
#include "pvr_ion_stats.h"
#include "di_common.h"
#include "di_server.h"
#include "img_types.h"
#include "pvr_debug.h"
#include "allocmem.h"
#include "lock_types.h"
#include "lock.h"
#include "osfunc.h"

#define MAX_NUM_HEAPS (5)

#define GET_ION_HEAP_ID(mask) (ilog2(mask))

/* It's better to check dma operations but ION dma_buf_ops is not exported. */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
#define ION_DMA_BUF_EXP_NAME "ion_dma_buf"
#else
#define ION_DMA_BUF_EXP_NAME "ion"
#endif

/* Android kernel common 5.4 support ion_query_heaps_kernel in ION. */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0)) && defined(ANDROID)
#define ION_HAS_QUERY_HEAPS_KERNEL
#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0)) && defined(ANDROID) */

typedef struct _pvr_ion_stats_heap_ {
	/* Name of Heap */
	char szName[32];
	/* An unique key which refers to a specific heap */
	IMG_UINT32 ui32HashKey;
} PVR_ION_STATS_HEAP;

typedef struct _pvr_ion_stats_buf_ {
	/* Node in a tree */
	struct rb_node node;
	/* Indicate the buffer was created from which heap */
	IMG_UINT32     ui32HeapKey;
	/* A key which represents the buffer in tree */
	uintptr_t      addr;
	/* Size of the buffer */
	size_t         uiBytes;

	/* used for debugging */
	struct task_struct *psTask;

	IMG_PID uiPID;
} PVR_ION_STATS_BUF;

typedef IMG_UINT32 (*HashKeypfn)(uintptr_t input);

typedef struct _pvr_ion_stats_state_ {
	/* Supported heaps on this platform */
	PVR_ION_STATS_HEAP sHeapData[MAX_NUM_HEAPS];
	IMG_UINT32 ui32NumHeaps;

	/* Nodes of debugfs */
	DI_GROUP *debugfs_ion;
	DI_GROUP *debugfs_heaps;
	DI_ENTRY *debugfs_heaps_entry[MAX_NUM_HEAPS];

	/* A record of buffers */
	struct rb_root buffers;

	/* Buffer lock that need to be held to edit tree */
	POS_LOCK hBuffersLock;

	/* Hash function for heap hash key */
	HashKeypfn pfnHashKey;
} PVR_ION_STATS_STATE;

/* forward declare */
static IMG_UINT32 StringHashFunc(uintptr_t input);
static IMG_UINT32 DefaultHashFunc(uintptr_t input);

static PVR_ION_STATS_STATE gPvrIonStatsState = {

	.pfnHashKey =
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 12, 0)) && \
	!defined(ION_HAS_QUERY_HEAPS_KERNEL)
		StringHashFunc,
#else
		DefaultHashFunc,
#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 12, 0)) && \
		  !defined(ION_HAS_QUERY_HEAPS_KERNEL) */
};

static IMG_BOOL isIonBuf(struct dma_buf *psDmaBuf)
{
	if (!strcmp(psDmaBuf->exp_name, ION_DMA_BUF_EXP_NAME))
		return IMG_TRUE;

	return IMG_FALSE;
}

static IMG_UINT32 pvr_ion_stats_query_heaps(PVR_ION_STATS_HEAP *heaps,
		HashKeypfn pfnHashKey)
{
	/* The heap id is good to be a hash key as it's unique. From Linux 4.12,
	 * ION id mask has been deprecated. Getting heap ids is only supported
	 * to query ION driver from userspace. It's not possible to get correct
	 * heap ids in kernel space and we're pretty sure ION_HEAP_NAME definitions
	 * are robust here as device won't boot if the name is incorrect. Thus,
	 * creating a key from heap name instead.
	 * For Android common 5.4, ion_query_heaps_kernel is supported.
	 */
#if defined(ION_HAS_QUERY_HEAPS_KERNEL)
	struct ion_heap_data sDefaultIonHeapsData[MAX_NUM_HEAPS];
#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 12, 0))
	PVR_ION_STATS_HEAP sDefaultIonHeaps[] = {
#if defined(ION_DEFAULT_HEAP_NAME)
		{
			.szName      = ION_DEFAULT_HEAP_NAME,
			.ui32HashKey = pfnHashKey((uintptr_t)ION_DEFAULT_HEAP_NAME),
		},
#endif /* defined(ION_DEFAULT_HEAP_NAME) */
#if defined(ION_FALLBACK_HEAP_NAME)
		{
			.szName      = ION_FALLBACK_HEAP_NAME,
			.ui32HashKey = pfnHashKey((uintptr_t)ION_FALLBACK_HEAP_NAME),
		}
#endif /* defined(ION_FALLBACK_HEAP_NAME) */
	};
#else
	PVR_ION_STATS_HEAP sDefaultIonHeaps[] = {
#if defined(ION_DEFAULT_HEAP_ID_MASK)
		{
			.szName      = ION_DEFAULT_HEAP_NAME,
			.ui32HashKey = GET_ION_HEAP_ID(ION_DEFAULT_HEAP_ID_MASK),
		},
#endif /* defined(ION_DEFAULT_HEAP_ID_MASK) */
#if defined(ION_FALLBACK_HEAP_ID_MASK)
		{
			.szName      = ION_FALLBACK_HEAP_NAME,
			.ui32HashKey = GET_ION_HEAP_ID(ION_FALLBACK_HEAP_ID_MASK),
		}
#endif /* defined(ION_FALLBACK_HEAP_ID_MASK) */
	};
#endif /* defined(ION_HAS_QUERY_HEAPS_KERNEL) */

	size_t numHeapsData;
	IMG_UINT32 i;

#if defined(ION_HAS_QUERY_HEAPS_KERNEL)
	numHeapsData = ion_query_heaps_kernel(sDefaultIonHeapsData,
			ARRAY_SIZE(sDefaultIonHeapsData));
	for (i = 0; i < numHeapsData; i++) {
		strlcpy(heaps[i].szName, sDefaultIonHeapsData[i].name, 32);
		heaps[i].ui32HashKey = pfnHashKey(sDefaultIonHeapsData[i].heap_id);
	}
#else
#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 12, 0))
	PVR_UNREFERENCED_PARAMETER(pfnHashKey);
#endif /* (LINUX_VERSION_CODE < KERNEL_VERSION(4, 12, 0)) */

	numHeapsData = ARRAY_SIZE(sDefaultIonHeaps);
	for (i = 0; i < numHeapsData; i++) {
		heaps[i] = sDefaultIonHeaps[i];
	}
#endif /* defined(ION_HAS_QUERY_HEAPS_KERNEL) */

	return numHeapsData;
}

static int pvr_ion_stats_show(OSDI_IMPL_ENTRY *s, void *v)
{
	PVR_ION_STATS_HEAP *psHeap = (PVR_ION_STATS_HEAP *)DIGetPrivData(s);
	PVR_ION_STATS_STATE *psState = &gPvrIonStatsState;
	PVR_ION_STATS_BUF *entry;
	size_t total_size = 0;
	struct rb_node *n;

	DIPrintf(s, "%16s %8s %18s %10s\n", "client", "pid", "address", "size");
	DIPuts(s, "-------------------------------------------------------\n");

	OSLockAcquire(psState->hBuffersLock);
	for (n = rb_first(&psState->buffers); n; n = rb_next(n)) {
		char task_comm[TASK_COMM_LEN];
		entry = rb_entry(n, PVR_ION_STATS_BUF, node);

		if (entry->ui32HeapKey != psHeap->ui32HashKey)
			continue;

		if (entry->psTask) {
			get_task_comm(task_comm, entry->psTask);
			DIPrintf(s, "%16s %8u 0x%016lx %10zu\n", task_comm,
					task_pid_nr(entry->psTask), entry->addr,
					entry->uiBytes);
		}
		else {
			DIPrintf(s, "%16s %8u 0x%016lx %10zu\n", "kernel", entry->uiPID,
					entry->addr, entry->uiBytes);
		}
		total_size += entry->uiBytes;
	}
	OSLockRelease(psState->hBuffersLock);

	DIPuts(s, "-------------------------------------------------------\n");
	DIPrintf(s, "%44s %10zu\n", "total", total_size);
	DIPuts(s, "-------------------------------------------------------\n");

	return 0;
}

static PVRSRV_ERROR
ion_stats_init(PVR_ION_STATS_HEAP *heaps, IMG_UINT32 ui32NumHeaps,
		DI_GROUP **iondir, DI_GROUP **heapsdir, DI_ENTRY *entry[])
{
	const DI_ITERATOR_CB iterator = {
		.pfnShow = pvr_ion_stats_show,
	};
	PVRSRV_ERROR eError;
	int i;

	if (!ui32NumHeaps) {
		/* No heaps are available, return success */
		return PVRSRV_OK;
	}

	if (*iondir) {
		PVR_DPF((PVR_DBG_ERROR, "ION debugfs already created."));
		eError = PVRSRV_ERROR_INVALID_PARAMS;
		goto err_out;
	}

	if (*heapsdir) {
		PVR_DPF((PVR_DBG_ERROR, "ion/heaps debugfs already created."));
		eError = PVRSRV_ERROR_INVALID_PARAMS;
		goto err_out;
	}

	eError = DICreateGroup("ion", NULL, iondir);
	if (eError != PVRSRV_OK) {
		PVR_DPF((PVR_DBG_ERROR, "Failed to create ion debugfs directory."));
		goto err_out;
	}

	eError = DICreateGroup("heaps", *iondir, heapsdir);
	if (eError != PVRSRV_OK) {
		PVR_DPF((PVR_DBG_ERROR, "Failed to create heaps debugfs directory."));
		goto err_destroy_ion_debugfs;
	}

	for (i = 0; i < ui32NumHeaps; i++) {
		eError = DICreateEntry(heaps[i].szName, *heapsdir, &iterator,
				&heaps[i], DI_ENTRY_TYPE_GENERIC, &entry[i]);
		if (eError != PVRSRV_OK) {
			PVR_DPF((PVR_DBG_ERROR, "Failed to create heaps %s entry.",
						heaps[i].szName));
			goto err_destroy_heaps_and_entry_debugfs;
		}
	}

err_out:
	return eError;
err_destroy_heaps_and_entry_debugfs:
	DIDestroyGroup(*heapsdir);
	while ((i--) > 0) {
		DIDestroyEntry(entry[i]);
	}
err_destroy_ion_debugfs:
	DIDestroyGroup(*iondir);
	goto err_out;
}

static PVR_ION_STATS_BUF *GetBuf(struct rb_root *root, uintptr_t addr)
{
	struct rb_node *entry = root->rb_node;
	PVR_ION_STATS_BUF *psBuf;

	while (entry != NULL) {
		psBuf = container_of(entry, PVR_ION_STATS_BUF, node);

		if (addr < psBuf->addr)
			entry = entry->rb_left;
		else if (addr > psBuf->addr)
			entry = entry->rb_right;
		else
			return psBuf;
	}

	return NULL;
}

static IMG_UINT32 StringHashFunc(uintptr_t input)
{
	/* Djb2 hash function */
	const char *szHeapName = (const char *)input;
	unsigned long hash = 5381;
	int c;

	while ((c = *szHeapName++))
	{
		hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
	}

	hash &= ~(1 << 31);

	return (IMG_UINT32)hash;
}

static IMG_UINT32 DefaultHashFunc(uintptr_t input)
{
	return (IMG_UINT32)input;
}

PVRSRV_ERROR PVRSRVIonStatsInitialise(void)
{
	PVR_ION_STATS_STATE *psState = &gPvrIonStatsState;
	PVRSRV_ERROR eError;

	psState->ui32NumHeaps = pvr_ion_stats_query_heaps(psState->sHeapData,
			psState->pfnHashKey);
	if (!psState->ui32NumHeaps) {
		PVR_DPF((PVR_DBG_WARNING, "No ION heaps are available."));
		return PVRSRV_OK;
	}

	eError = OSLockCreateNoStats(&psState->hBuffersLock);
	if (eError != PVRSRV_OK) {
		PVR_DPF((PVR_DBG_ERROR, "Failed to create buffers lock."));
		goto err_out;
	}

	eError = ion_stats_init(psState->sHeapData, psState->ui32NumHeaps,
			&psState->debugfs_ion, &psState->debugfs_heaps,
			psState->debugfs_heaps_entry);
	if (eError != PVRSRV_OK)
		goto err_destroy_lock;

	psState->buffers = RB_ROOT;

err_out:
	return eError;
err_destroy_lock:
	OSLockDestroyNoStats(psState->hBuffersLock);
	goto err_out;
}

void PVRSRVIonStatsDestroy(void)
{
	PVR_ION_STATS_STATE *psState = &gPvrIonStatsState;
	PVR_ION_STATS_BUF *entry;
	struct rb_node *n;
	int i;

	/* Cleanup nodes if any */
	OSLockAcquire(psState->hBuffersLock);
	for (n = rb_first(&psState->buffers); n; n = rb_next(n)) {
		entry = rb_entry(n, PVR_ION_STATS_BUF, node);
		rb_erase(&entry->node, &psState->buffers);
		dma_buf_put((struct dma_buf *)entry->addr);
		OSFreeMemNoStats(entry);
	}
	OSLockRelease(psState->hBuffersLock);

	for (i = 0; i < psState->ui32NumHeaps; i++) {
		DIDestroyEntry(psState->debugfs_heaps_entry[i]);
		psState->debugfs_heaps_entry[i] = NULL;
	}

	if (psState->debugfs_heaps) {
		DIDestroyGroup(psState->debugfs_heaps);
		psState->debugfs_heaps = NULL;
	}

	if (psState->debugfs_ion) {
		DIDestroyGroup(psState->debugfs_ion);
		psState->debugfs_ion = NULL;
	}

	if (psState->hBuffersLock) {
		/* Destroy the lock */
		OSLockDestroyNoStats(psState->hBuffersLock);
		psState->hBuffersLock = NULL;
	}
}

void PVRSRVIonAddMemAllocRecord(struct dma_buf *psDmaBuf)
{
	PVR_ION_STATS_STATE *psState = &gPvrIonStatsState;
	struct rb_node **p = &psState->buffers.rb_node;
	HashKeypfn pfnHashKey = psState->pfnHashKey;
	PVR_ION_STATS_BUF *psBuf, *entry;
	struct rb_node *parent = NULL;
	struct ion_buffer *psIonBuf;
	struct task_struct *psTask;

	if (!psState->ui32NumHeaps)
		return;

	if (!psDmaBuf) {
		PVR_DPF((PVR_DBG_ERROR, "Invalid dma buffer"));
		return;
	}

	/* We're only interested in ION buffers */
	if (isIonBuf(psDmaBuf) == IMG_FALSE)
		return;

	psIonBuf = (struct ion_buffer *)psDmaBuf->priv;
	if (!psIonBuf) {
		PVR_DPF((PVR_DBG_ERROR, "Invalid ION buffer"));
		return;
	}

	psBuf = OSAllocZMemNoStats(sizeof(PVR_ION_STATS_BUF));
	if (!psBuf) {
		PVR_DPF((PVR_DBG_ERROR, "Failed to allocate memory."));
		return;
	}

	if (pfnHashKey == StringHashFunc) {
		psBuf->ui32HeapKey = pfnHashKey((uintptr_t)psIonBuf->heap->name);
		PVR_UNREFERENCED_PARAMETER(DefaultHashFunc);
	}
	else
		psBuf->ui32HeapKey = pfnHashKey((uintptr_t)psIonBuf->heap->id);

	psBuf->addr = (uintptr_t)psDmaBuf;
	psBuf->uiBytes = psDmaBuf->size;

	get_task_struct(current->group_leader);
	task_lock(current->group_leader);

	psTask = current->group_leader;
	psBuf->uiPID = task_pid_nr(psTask);

	if (current->group_leader->flags & PF_KTHREAD) {
		put_task_struct(current->group_leader);
		psTask = NULL;
	}

	psBuf->psTask = psTask;

	task_unlock(current->group_leader);

	OSLockAcquire(psState->hBuffersLock);
	while (*p) {
		parent = *p;
		entry = rb_entry(parent, PVR_ION_STATS_BUF, node);

		if (psBuf->addr < entry->addr) {
			p = &(*p)->rb_left;
		} else if (psBuf->addr > entry->addr) {
			p = &(*p)->rb_right;
		} else {
			/* addr should be unique. */
			PVR_DPF((PVR_DBG_ERROR, "Buffer have already in tree"));
			PVR_ASSERT(0);
		}
	}

	rb_link_node(&psBuf->node, parent, p);
	rb_insert_color(&psBuf->node, &psState->buffers);
	/* Take a ref count on this buffer in case somehow it's released */
	get_dma_buf(psDmaBuf);
	OSLockRelease(psState->hBuffersLock);
}

void PVRSRVIonRemoveMemAllocRecord(struct dma_buf *psDmaBuf)
{
	PVR_ION_STATS_STATE *psState = &gPvrIonStatsState;
	PVR_ION_STATS_BUF *psBuf;

	if (!psDmaBuf) {
		PVR_DPF((PVR_DBG_ERROR, "Invalid dma buffer"));
		return;
	}

	/* We're only interested in ION buffers */
	if (isIonBuf(psDmaBuf) == IMG_FALSE)
		return;

	OSLockAcquire(psState->hBuffersLock);
	psBuf = GetBuf(&psState->buffers, (uintptr_t)psDmaBuf);
	if (!psBuf) {
		PVR_DPF((PVR_DBG_ERROR, "Failed to find dma buffer"));
		goto out;
	}

	rb_erase(&psBuf->node, &psState->buffers);

	dma_buf_put(psDmaBuf);

	if (psBuf->psTask)
		put_task_struct(psBuf->psTask);

	OSFreeMemNoStats(psBuf);

out:
	OSLockRelease(psState->hBuffersLock);
}
