/* Copyright (c) 2016-2017, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/ipa.h>
#include "../ipa_v3/ipa_i.h"
#include "ipa_ut_framework.h"

#define IPA_TEST_DMA_WQ_NAME_BUFF_SZ		64
#define IPA_TEST_DMA_MT_TEST_NUM_WQ		400
#define IPA_TEST_DMA_MEMCPY_BUFF_SIZE		16384
#define IPA_TEST_DMA_MAX_PKT_SIZE		0xFF00
#define IPA_DMA_TEST_LOOP_NUM			1000
#define IPA_DMA_TEST_INT_LOOP_NUM		50
#define IPA_DMA_TEST_ASYNC_PARALLEL_LOOP_NUM	128
#define IPA_DMA_RUN_TEST_UNIT_IN_LOOP(test_unit, iters, rc, args...)	\
	do {								\
		int __i;						\
		for (__i = 0; __i < iters; __i++) {	\
			IPA_UT_LOG(#test_unit " START iter %d\n", __i);	\
			rc = test_unit(args);				\
			if (!rc)					\
				continue;				\
			IPA_UT_LOG(#test_unit " failed %d\n", rc);	\
			break;						\
		}							\
	} while (0)

/**
 * struct ipa_test_dma_async_user_data - user_data structure for async memcpy
 * @src_mem: source memory buffer
 * @dest_mem: destination memory buffer
 * @call_serial_number: Id of the caller
 * @copy_done: Completion object
 */
struct ipa_test_dma_async_user_data {
	struct ipa_mem_buffer src_mem;
	struct ipa_mem_buffer dest_mem;
	int call_serial_number;
	struct completion copy_done;
};

/**
 * ipa_test_dma_setup() - Suite setup function
 */
static int ipa_test_dma_setup(void **ppriv)
{
	int rc;

	IPA_UT_DBG("Start Setup\n");

	if (!ipa3_ctx) {
		IPA_UT_ERR("No IPA ctx\n");
		return -EINVAL;
	}

	rc = ipa_dma_init();
	if (rc)
		IPA_UT_ERR("Fail to init ipa_dma - return code %d\n", rc);
	else
		IPA_UT_DBG("ipa_dma_init() Completed successfully!\n");

	*ppriv = NULL;

	return rc;
}

/**
 * ipa_test_dma_teardown() - Suite teardown function
 */
static int ipa_test_dma_teardown(void *priv)
{
	IPA_UT_DBG("Start Teardown\n");
	ipa_dma_destroy();
	return 0;
}

static int ipa_test_dma_alloc_buffs(struct ipa_mem_buffer *src,
				struct ipa_mem_buffer *dest,
				int size)
{
	int i;
	static int val = 1;
	int rc;

	val++;
	src->size = size;
	src->base = dma_alloc_coherent(ipa3_ctx->pdev, src->size,
				       &src->phys_base, GFP_KERNEL);
	if (!src->base) {
		IPA_UT_LOG("fail to alloc dma mem %d bytes\n", size);
		IPA_UT_TEST_FAIL_REPORT("fail to alloc dma mem");
		return -ENOMEM;
	}

	dest->size = size;
	dest->base = dma_alloc_coherent(ipa3_ctx->pdev, dest->size,
					&dest->phys_base, GFP_KERNEL);
	if (!dest->base) {
		IPA_UT_LOG("fail to alloc dma mem %d bytes\n", size);
		IPA_UT_TEST_FAIL_REPORT("fail to alloc dma mem");
		rc = -ENOMEM;
		goto fail_alloc_dest;
	}

	memset(dest->base, 0, dest->size);
	for (i = 0; i < src->size; i++)
		memset(src->base + i, (val + i) & 0xFF, 1);
	rc = memcmp(dest->base, src->base, dest->size);
	if (rc == 0) {
		IPA_UT_LOG("dest & src buffers are equal\n");
		IPA_UT_TEST_FAIL_REPORT("dest & src buffers are equal");
		rc = -EFAULT;
		goto fail_buf_cmp;
	}

	return 0;

fail_buf_cmp:
	dma_free_coherent(ipa3_ctx->pdev, dest->size, dest->base,
		dest->phys_base);
fail_alloc_dest:
	dma_free_coherent(ipa3_ctx->pdev, src->size, src->base,
		src->phys_base);
	return rc;
}

static void ipa_test_dma_destroy_buffs(struct ipa_mem_buffer *src,
				struct ipa_mem_buffer *dest)
{
	dma_free_coherent(ipa3_ctx->pdev, src->size, src->base,
		src->phys_base);
	dma_free_coherent(ipa3_ctx->pdev, dest->size, dest->base,
		dest->phys_base);
}

/**
 * ipa_test_dma_memcpy_sync() - memcpy in sync mode
 *
 * @size: buffer size
 * @expect_fail: test expects the memcpy to fail
 *
 * To be run during tests
 * 1. Alloc src and dst buffers
 * 2. sync memcpy src to dst via dma
 * 3. compare src and dts if memcpy succeeded as expected
 */
static int ipa_test_dma_memcpy_sync(int size, bool expect_fail)
{
	int rc = 0;
	int i;
	struct ipa_mem_buffer src_mem;
	struct ipa_mem_buffer dest_mem;
	u8 *src;
	u8 *dest;

	rc = ipa_test_dma_alloc_buffs(&src_mem, &dest_mem, size);
	if (rc) {
		IPA_UT_LOG("fail to alloc buffers\n");
		IPA_UT_TEST_FAIL_REPORT("fail to alloc buffers");
		return rc;
	}

	rc = ipa_dma_sync_memcpy(dest_mem.phys_base, src_mem.phys_base, size);
	if (!expect_fail && rc) {
		IPA_UT_LOG("fail to sync memcpy - rc = %d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("sync memcpy failed");
		goto free_buffs;
	}
	if (expect_fail && !rc) {
		IPA_UT_LOG("sync memcpy succeeded while expected to fail\n");
		IPA_UT_TEST_FAIL_REPORT(
			"sync memcpy succeeded while expected to fail");
		rc = -EFAULT;
		goto free_buffs;
	}

	if (!rc) {
		/* if memcpy succeeded, compare the buffers */
		rc = memcmp(dest_mem.base, src_mem.base, size);
		if (rc) {
			IPA_UT_LOG("BAD memcpy - buffs are not equals\n");
			IPA_UT_TEST_FAIL_REPORT(
				"BAD memcpy - buffs are not equals");
			src = src_mem.base;
			dest = dest_mem.base;
			for (i = 0; i < size; i++)  {
				if (*(src + i) != *(dest + i)) {
					IPA_UT_LOG("byte: %d 0x%x != 0x%x\n",
						i, *(src + i), *(dest + i));
				}
			}
		}
	} else {
		/* if memcpy failed as expected, update the rc */
		rc = 0;
	}

free_buffs:
	ipa_test_dma_destroy_buffs(&src_mem, &dest_mem);
	return rc;
}

static void ipa_test_dma_async_memcpy_cb(void *comp_obj)
{
	struct completion *xfer_done;

	if (!comp_obj) {
		IPA_UT_ERR("Invalid Input\n");
		return;
	}
	xfer_done = (struct completion *)comp_obj;
	complete(xfer_done);
}

static void ipa_test_dma_async_memcpy_cb_user_data(void *user_param)
{
	int rc;
	int i;
	u8 *src;
	u8 *dest;
	struct ipa_test_dma_async_user_data *udata =
		(struct ipa_test_dma_async_user_data *)user_param;

	if (!udata) {
		IPA_UT_ERR("Invalid user param\n");
		return;
	}

	rc = memcmp(udata->dest_mem.base, udata->src_mem.base,
		udata->src_mem.size);
	if (rc) {
		IPA_UT_LOG("BAD memcpy - buffs are not equal sn=%d\n",
			udata->call_serial_number);
		IPA_UT_TEST_FAIL_REPORT(
			"BAD memcpy - buffs are not equal");
		src = udata->src_mem.base;
		dest = udata->dest_mem.base;
		for (i = 0; i < udata->src_mem.size; i++)  {
			if (*(src + i) != *(dest + i)) {
				IPA_UT_ERR("byte: %d 0x%x != 0x%x\n", i,
					   *(src + i), *(dest + i));
			}
		}
		return;
	}

	IPA_UT_LOG("Notify on async memcopy sn=%d\n",
		udata->call_serial_number);
	complete(&(udata->copy_done));
}

/**
 * ipa_test_dma_memcpy_async() - memcpy in async mode
 *
 * @size: buffer size
 * @expect_fail: test expected the memcpy to fail
 *
 * To be run during tests
 * 1. Alloc src and dst buffers
 * 2. async memcpy src to dst via dma and wait for completion
 * 3. compare src and dts if memcpy succeeded as expected
 */
static int ipa_test_dma_memcpy_async(int size, bool expect_fail)
{
	int rc = 0;
	int i;
	struct ipa_mem_buffer src_mem;
	struct ipa_mem_buffer dest_mem;
	u8 *src;
	u8 *dest;
	struct completion xfer_done;

	rc = ipa_test_dma_alloc_buffs(&src_mem, &dest_mem, size);
	if (rc) {
		IPA_UT_LOG("fail to alloc buffers\n");
		IPA_UT_TEST_FAIL_REPORT("fail to alloc buffers");
		return rc;
	}

	init_completion(&xfer_done);
	rc = ipa_dma_async_memcpy(dest_mem.phys_base, src_mem.phys_base, size,
		ipa_test_dma_async_memcpy_cb, &xfer_done);
	if (!expect_fail && rc) {
		IPA_UT_LOG("fail to initiate async memcpy - rc=%d\n",
			rc);
		IPA_UT_TEST_FAIL_REPORT("async memcpy initiate failed");
		goto free_buffs;
	}
	if (expect_fail && !rc) {
		IPA_UT_LOG("async memcpy succeeded while expected to fail\n");
		IPA_UT_TEST_FAIL_REPORT(
			"async memcpy succeeded while expected to fail");
		rc = -EFAULT;
		goto free_buffs;
	}

	if (!rc) {
		/* if memcpy succeeded, compare the buffers */
		wait_for_completion(&xfer_done);
		rc = memcmp(dest_mem.base, src_mem.base, size);
		if (rc) {
			IPA_UT_LOG("BAD memcpy - buffs are not equals\n");
			IPA_UT_TEST_FAIL_REPORT(
				"BAD memcpy - buffs are not equals");
			src = src_mem.base;
			dest = dest_mem.base;
			for (i = 0; i < size; i++)  {
				if (*(src + i) != *(dest + i)) {
					IPA_UT_LOG("byte: %d 0x%x != 0x%x\n",
						i, *(src + i), *(dest + i));
				}
			}
		}
	} else {
		/* if memcpy failed as expected, update the rc */
		rc = 0;
	}

free_buffs:
	ipa_test_dma_destroy_buffs(&src_mem, &dest_mem);
	return rc;
}

/**
 * ipa_test_dma_sync_async_memcpy() - memcpy in sync and then async mode
 *
 * @size: buffer size
 *
 * To be run during tests
 * 1. several sync memcopy in row
 * 2. several async memcopy -
 *	back-to-back (next async try initiated after prev is completed)
 */
static int ipa_test_dma_sync_async_memcpy(int size)
{
	int rc;

	IPA_DMA_RUN_TEST_UNIT_IN_LOOP(ipa_test_dma_memcpy_sync,
		IPA_DMA_TEST_INT_LOOP_NUM, rc, size, false);
	if (rc) {
		IPA_UT_LOG("sync memcopy fail rc=%d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("sync memcopy fail");
		return rc;
	}

	IPA_DMA_RUN_TEST_UNIT_IN_LOOP(ipa_test_dma_memcpy_async,
		IPA_DMA_TEST_INT_LOOP_NUM, rc, size, false);
	if (rc) {
		IPA_UT_LOG("async memcopy fail rc=%d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("async memcopy fail");
		return rc;
	}

	return 0;
}

/**
 * TEST: test enable/disable dma
 *	1. enable dma
 *	2. disable dma
 */
static int ipa_test_dma_enable_disable(void *priv)
{
	int rc;

	IPA_UT_LOG("Test Start\n");

	rc = ipa_dma_enable();
	if (rc) {
		IPA_UT_LOG("DMA enable failed rc=%d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("fail enable dma");
		return rc;
	}

	rc = ipa_dma_disable();
	if (rc) {
		IPA_UT_LOG("DMA disable failed rc=%d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("fail disable dma");
		return rc;
	}

	return 0;
}

/**
 * TEST: test init/enable/disable/destroy dma
 *	1. init dma
 *	2. enable dma
 *	3. disable dma
 *	4. destroy dma
 */
static int ipa_test_dma_init_enbl_disable_destroy(void *priv)
{
	int rc;

	IPA_UT_LOG("Test Start\n");

	rc = ipa_dma_init();
	if (rc) {
		IPA_UT_LOG("DMA Init failed rc=%d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("fail init dma");
		return rc;
	}

	rc = ipa_dma_enable();
	if (rc) {
		ipa_dma_destroy();
		IPA_UT_LOG("DMA enable failed rc=%d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("fail enable dma");
		return rc;
	}

	rc = ipa_dma_disable();
	if (rc) {
		IPA_UT_LOG("DMA disable failed rc=%d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("fail disable dma");
		return rc;
	}

	ipa_dma_destroy();

	return 0;
}

/**
 * TEST: test enablex2/disablex2 dma
 *	1. enable dma
 *	2. enable dma
 *	3. disable dma
 *	4. disable dma
 */
static int ipa_test_dma_enblx2_disablex2(void *priv)
{
	int rc;

	IPA_UT_LOG("Test Start\n");

	rc = ipa_dma_enable();
	if (rc) {
		ipa_dma_destroy();
		IPA_UT_LOG("DMA enable failed rc=%d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("fail enable dma");
		return rc;
	}

	rc = ipa_dma_enable();
	if (rc) {
		ipa_dma_destroy();
		IPA_UT_LOG("DMA enable failed rc=%d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("fail enable dma");
		return rc;
	}

	rc = ipa_dma_disable();
	if (rc) {
		IPA_UT_LOG("DMA disable failed rc=%d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("fail disable dma");
		return rc;
	}

	rc = ipa_dma_disable();
	if (rc) {
		IPA_UT_LOG("DMA disable failed rc=%d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("fail disable dma");
		return rc;
	}

	return 0;
}

/**
 * TEST: memcpy before dma enable
 *
 *	1. sync memcpy - should fail
 *	2. async memcpy - should fail
 */
static int ipa_test_dma_memcpy_before_enable(void *priv)
{
	int rc;

	IPA_UT_LOG("Test Start\n");

	rc = ipa_test_dma_memcpy_sync(IPA_TEST_DMA_MEMCPY_BUFF_SIZE, true);
	if (rc) {
		IPA_UT_LOG("sync memcpy succeeded unexpectedly rc=%d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("sync memcpy succeeded unexpectedly");
		return rc;
	}

	rc = ipa_test_dma_memcpy_async(IPA_TEST_DMA_MEMCPY_BUFF_SIZE, true);
	if (rc) {
		IPA_UT_LOG("async memcpy succeeded unexpectedly rc=%d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("sync memcpy succeeded unexpectedly");
		return rc;
	}

	return 0;
}

/**
 * TEST: Sync memory copy
 *
 *	1. dma enable
 *	2. sync memcpy
 *	3. dma disable
 */
static int ipa_test_dma_sync_memcpy(void *priv)
{
	int rc;

	IPA_UT_LOG("Test Start\n");

	rc = ipa_dma_enable();
	if (rc) {
		IPA_UT_LOG("DMA enable failed rc=%d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("fail enable dma");
		return rc;
	}

	rc = ipa_test_dma_memcpy_sync(IPA_TEST_DMA_MEMCPY_BUFF_SIZE, false);
	if (rc) {
		IPA_UT_LOG("sync memcpy failed rc=%d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("sync memcpy failed");
		(void)ipa_dma_disable();
		return rc;
	}

	rc = ipa_dma_disable();
	if (rc) {
		IPA_UT_LOG("DMA disable failed rc=%d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("fail disable dma");
		return rc;
	}

	return 0;
}

/**
* TEST: Small sync memory copy
*
*	1. dma enable
*	2. small sync memcpy
*	3. small sync memcpy
*	4. dma disable
*/
static int ipa_test_dma_sync_memcpy_small(void *priv)
{
	int rc;

	IPA_UT_LOG("Test Start\n");

	rc = ipa_dma_enable();
	if (rc) {
		IPA_UT_LOG("DMA enable failed rc=%d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("fail enable dma");
		return rc;
	}

	rc = ipa_test_dma_memcpy_sync(4, false);
	if (rc) {
		IPA_UT_LOG("sync memcpy failed rc=%d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("sync memcpy failed");
		(void)ipa_dma_disable();
		return rc;
	}

	rc = ipa_test_dma_memcpy_sync(7, false);
	if (rc) {
		IPA_UT_LOG("sync memcpy failed rc=%d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("sync memcpy failed");
		(void)ipa_dma_disable();
		return rc;
	}

	rc = ipa_dma_disable();
	if (rc) {
		IPA_UT_LOG("DMA disable failed rc=%d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("fail disable dma");
		return rc;
	}

	return 0;
}

/**
 * TEST: Async memory copy
 *
 *	1. dma enable
 *	2. async memcpy
 *	3. dma disable
 */
static int ipa_test_dma_async_memcpy(void *priv)
{
	int rc;

	IPA_UT_LOG("Test Start\n");

	rc = ipa_dma_enable();
	if (rc) {
		IPA_UT_LOG("DMA enable failed rc=%d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("fail enable dma");
		return rc;
	}

	rc = ipa_test_dma_memcpy_async(IPA_TEST_DMA_MEMCPY_BUFF_SIZE, false);
	if (rc) {
		IPA_UT_LOG("async memcpy failed rc=%d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("async memcpy failed");
		(void)ipa_dma_disable();
		return rc;
	}

	rc = ipa_dma_disable();
	if (rc) {
		IPA_UT_LOG("DMA disable failed rc=%d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("fail disable dma");
		return rc;
	}

	return 0;
}

/**
 * TEST: Small async memory copy
 *
 *	1. dma enable
 *	2. async memcpy
 *	3. async memcpy
 *	4. dma disable
 */
static int ipa_test_dma_async_memcpy_small(void *priv)
{
	int rc;

	IPA_UT_LOG("Test Start\n");

	rc = ipa_dma_enable();
	if (rc) {
		IPA_UT_LOG("DMA enable failed rc=%d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("fail enable dma");
		return rc;
	}

	rc = ipa_test_dma_memcpy_async(4, false);
	if (rc) {
		IPA_UT_LOG("async memcpy failed rc=%d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("async memcpy failed");
		(void)ipa_dma_disable();
		return rc;
	}

	rc = ipa_test_dma_memcpy_async(7, false);
	if (rc) {
		IPA_UT_LOG("async memcpy failed rc=%d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("async memcpy failed");
		(void)ipa_dma_disable();
		return rc;
	}

	rc = ipa_dma_disable();
	if (rc) {
		IPA_UT_LOG("DMA disable failed rc=%d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("fail disable dma");
		return rc;
	}

	return 0;
}

/**
 * TEST: Iteration of sync memory copy
 *
 *	1. dma enable
 *	2. sync memcpy in loop - in row
 *	3. dma disable
 */
static int ipa_test_dma_sync_memcpy_in_loop(void *priv)
{
	int rc;

	IPA_UT_LOG("Test Start\n");

	rc = ipa_dma_enable();
	if (rc) {
		IPA_UT_LOG("DMA enable failed rc=%d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("fail enable dma");
		return rc;
	}

	IPA_DMA_RUN_TEST_UNIT_IN_LOOP(ipa_test_dma_memcpy_sync,
		IPA_DMA_TEST_LOOP_NUM, rc,
		IPA_TEST_DMA_MEMCPY_BUFF_SIZE, false);
	if (rc) {
		IPA_UT_LOG("Iterations of sync memcpy failed rc=%d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("Iterations of sync memcpy failed");
		(void)ipa_dma_disable();
		return rc;
	}

	rc = ipa_dma_disable();
	if (rc) {
		IPA_UT_LOG("DMA disable failed rc=%d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("fail disable dma");
		return rc;
	}

	return 0;
}

/**
 * TEST: Iteration of async memory copy
 *
 *	1. dma enable
 *	2. async memcpy in loop - back-to-back
 *		next async copy is initiated once previous one completed
 *	3. dma disable
 */
static int ipa_test_dma_async_memcpy_in_loop(void *priv)
{
	int rc;

	IPA_UT_LOG("Test Start\n");

	rc = ipa_dma_enable();
	if (rc) {
		IPA_UT_LOG("DMA enable failed rc=%d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("fail enable dma");
		return rc;
	}

	IPA_DMA_RUN_TEST_UNIT_IN_LOOP(ipa_test_dma_memcpy_async,
		IPA_DMA_TEST_LOOP_NUM, rc,
		IPA_TEST_DMA_MEMCPY_BUFF_SIZE, false);
	if (rc) {
		IPA_UT_LOG("Iterations of async memcpy failed rc=%d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("Iterations of async memcpy failed");
		(void)ipa_dma_disable();
		return rc;
	}

	rc = ipa_dma_disable();
	if (rc) {
		IPA_UT_LOG("DMA disable failed rc=%d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("fail disable dma");
		return rc;
	}

	return 0;
}

/**
 * TEST: Iteration of interleaved sync and async memory copy
 *
 *	1. dma enable
 *	2. sync and async memcpy in loop - interleaved
 *	3. dma disable
 */
static int ipa_test_dma_interleaved_sync_async_memcpy_in_loop(void *priv)
{
	int rc;

	IPA_UT_LOG("Test Start\n");

	rc = ipa_dma_enable();
	if (rc) {
		IPA_UT_LOG("DMA enable failed rc=%d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("fail enable dma");
		return rc;
	}

	IPA_DMA_RUN_TEST_UNIT_IN_LOOP(ipa_test_dma_sync_async_memcpy,
		IPA_DMA_TEST_INT_LOOP_NUM, rc,
		IPA_TEST_DMA_MEMCPY_BUFF_SIZE);
	if (rc) {
		IPA_UT_LOG(
			"Iterations of interleaved sync async memcpy failed rc=%d\n"
			, rc);
		IPA_UT_TEST_FAIL_REPORT(
			"Iterations of interleaved sync async memcpy failed");
		(void)ipa_dma_disable();
		return rc;
	}

	rc = ipa_dma_disable();
	if (rc) {
		IPA_UT_LOG("DMA disable failed rc=%d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("fail disable dma");
		return rc;
	}

	return 0;
}

static atomic_t ipa_test_dma_mt_test_pass;

struct one_memcpy_work {
	struct work_struct work_s;
	int size;
};

static void ipa_test_dma_wrapper_test_one_sync(struct work_struct *work)
{
	int rc;
	struct one_memcpy_work *data =
		container_of(work, struct one_memcpy_work, work_s);

	rc = ipa_test_dma_memcpy_sync(data->size, false);
	if (rc) {
		IPA_UT_LOG("fail sync memcpy from thread rc=%d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("fail sync memcpy from thread");
		return;
	}
	atomic_inc(&ipa_test_dma_mt_test_pass);
}

static void ipa_test_dma_wrapper_test_one_async(struct work_struct *work)
{
	int rc;
	struct one_memcpy_work *data =
		container_of(work, struct one_memcpy_work, work_s);

	rc = ipa_test_dma_memcpy_async(data->size, false);
	if (rc) {
		IPA_UT_LOG("fail async memcpy from thread rc=%d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("fail async memcpy from thread");
		return;
	}
	atomic_inc(&ipa_test_dma_mt_test_pass);
}

/**
 * TEST: Multiple threads running sync and sync mem copy
 *
 *	1. dma enable
 *	2. In-loop
 *		2.1 create wq for sync memcpy
 *		2.2 create wq for async memcpy
 *		2.3 queue sync memcpy work
 *		2.4 queue async memcoy work
 *	3. In-loop
 *		3.1 flush and destroy wq sync
 *		3.2 flush and destroy wq async
 *	3. dma disable
 */
static int ipa_test_dma_mt_sync_async(void *priv)
{
	int rc;
	int i;
	static struct workqueue_struct *wq_sync[IPA_TEST_DMA_MT_TEST_NUM_WQ];
	static struct workqueue_struct *wq_async[IPA_TEST_DMA_MT_TEST_NUM_WQ];
	static struct one_memcpy_work async[IPA_TEST_DMA_MT_TEST_NUM_WQ];
	static struct one_memcpy_work sync[IPA_TEST_DMA_MT_TEST_NUM_WQ];
	char buff[IPA_TEST_DMA_WQ_NAME_BUFF_SZ];

	memset(wq_sync, 0, sizeof(wq_sync));
	memset(wq_sync, 0, sizeof(wq_async));
	memset(async, 0, sizeof(async));
	memset(sync, 0, sizeof(sync));

	rc = ipa_dma_enable();
	if (rc) {
		IPA_UT_LOG("DMA enable failed rc=%d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("fail enable dma");
		return rc;
	}

	atomic_set(&ipa_test_dma_mt_test_pass, 0);
	for (i = 0; i < IPA_TEST_DMA_MT_TEST_NUM_WQ; i++) {
		snprintf(buff, sizeof(buff), "ipa_test_dmaSwq%d", i);
		wq_sync[i] = create_singlethread_workqueue(buff);
		if (!wq_sync[i]) {
			IPA_UT_ERR("failed to create sync wq#%d\n", i);
			rc = -EFAULT;
			goto fail_create_wq;
		}
		snprintf(buff, IPA_RESOURCE_NAME_MAX, "ipa_test_dmaAwq%d", i);
		wq_async[i] = create_singlethread_workqueue(buff);
		if (!wq_async[i]) {
			IPA_UT_ERR("failed to create async wq#%d\n", i);
			rc = -EFAULT;
			goto fail_create_wq;
		}

		if (i % 2) {
			sync[i].size = IPA_TEST_DMA_MEMCPY_BUFF_SIZE;
			async[i].size = IPA_TEST_DMA_MEMCPY_BUFF_SIZE;
		} else {
			sync[i].size = 4;
			async[i].size = 4;
		}
		INIT_WORK(&sync[i].work_s, ipa_test_dma_wrapper_test_one_sync);
		queue_work(wq_sync[i], &sync[i].work_s);
		INIT_WORK(&async[i].work_s,
			ipa_test_dma_wrapper_test_one_async);
		queue_work(wq_async[i], &async[i].work_s);
	}

	for (i = 0; i < IPA_TEST_DMA_MT_TEST_NUM_WQ; i++) {
		flush_workqueue(wq_sync[i]);
		destroy_workqueue(wq_sync[i]);
		flush_workqueue(wq_async[i]);
		destroy_workqueue(wq_async[i]);
	}

	rc = ipa_dma_disable();
	if (rc) {
		IPA_UT_LOG("DMA disable failed rc=%d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("fail disable dma");
		return rc;
	}

	if ((2 * IPA_TEST_DMA_MT_TEST_NUM_WQ) !=
		atomic_read(&ipa_test_dma_mt_test_pass)) {
		IPA_UT_LOG(
			"Multi-threaded sync/async memcopy failed passed=%d\n"
			, atomic_read(&ipa_test_dma_mt_test_pass));
		IPA_UT_TEST_FAIL_REPORT(
			"Multi-threaded sync/async memcopy failed");
		return -EFAULT;
	}

	return 0;

fail_create_wq:
	(void)ipa_dma_disable();
	for (i = 0; i < IPA_TEST_DMA_MT_TEST_NUM_WQ; i++) {
		if (wq_sync[i])
			destroy_workqueue(wq_sync[i]);
		if (wq_async[i])
			destroy_workqueue(wq_async[i]);
	}

	return rc;
}

/**
 * TEST: Several parallel async memory copy iterations
 *
 *	1. create several user_data structures - one per iteration
 *	2. allocate buffs. Give slice for each iteration
 *	3. iterations of async mem copy
 *	4. wait for all to complete
 *	5. dma disable
 */
static int ipa_test_dma_parallel_async_memcpy_in_loop(void *priv)
{
	int rc;
	struct ipa_test_dma_async_user_data *udata;
	struct ipa_mem_buffer all_src_mem;
	struct ipa_mem_buffer all_dest_mem;
	int i;
	bool is_fail = false;

	IPA_UT_LOG("Test Start\n");

	rc = ipa_dma_enable();
	if (rc) {
		IPA_UT_LOG("DMA enable failed rc=%d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("fail enable dma");
		return rc;
	}

	udata = kzalloc(IPA_DMA_TEST_ASYNC_PARALLEL_LOOP_NUM *
		sizeof(struct ipa_test_dma_async_user_data), GFP_KERNEL);
	if (!udata) {
		IPA_UT_ERR("fail allocate user_data array\n");
		(void)ipa_dma_disable();
		return -ENOMEM;
	}

	rc = ipa_test_dma_alloc_buffs(&all_src_mem, &all_dest_mem,
		IPA_TEST_DMA_MEMCPY_BUFF_SIZE);
	if (rc) {
		IPA_UT_LOG("fail to alloc buffers\n");
		IPA_UT_TEST_FAIL_REPORT("fail to alloc buffers");
		kfree(udata);
		(void)ipa_dma_disable();
		return rc;
	}

	for (i = 0 ; i < IPA_DMA_TEST_ASYNC_PARALLEL_LOOP_NUM ; i++) {
		udata[i].src_mem.size =
			IPA_TEST_DMA_MEMCPY_BUFF_SIZE /
			IPA_DMA_TEST_ASYNC_PARALLEL_LOOP_NUM;
		udata[i].src_mem.base = all_src_mem.base + i *
			(IPA_TEST_DMA_MEMCPY_BUFF_SIZE /
			IPA_DMA_TEST_ASYNC_PARALLEL_LOOP_NUM);
		udata[i].src_mem.phys_base = all_src_mem.phys_base + i *
			(IPA_TEST_DMA_MEMCPY_BUFF_SIZE /
			IPA_DMA_TEST_ASYNC_PARALLEL_LOOP_NUM);

		udata[i].dest_mem.size =
			(IPA_TEST_DMA_MEMCPY_BUFF_SIZE /
			IPA_DMA_TEST_ASYNC_PARALLEL_LOOP_NUM);
		udata[i].dest_mem.base = all_dest_mem.base + i *
			(IPA_TEST_DMA_MEMCPY_BUFF_SIZE /
			IPA_DMA_TEST_ASYNC_PARALLEL_LOOP_NUM);
		udata[i].dest_mem.phys_base = all_dest_mem.phys_base + i *
			(IPA_TEST_DMA_MEMCPY_BUFF_SIZE /
			IPA_DMA_TEST_ASYNC_PARALLEL_LOOP_NUM);

		udata[i].call_serial_number = i + 1;
		init_completion(&(udata[i].copy_done));
		rc = ipa_dma_async_memcpy(udata[i].dest_mem.phys_base,
			udata[i].src_mem.phys_base,
			(IPA_TEST_DMA_MEMCPY_BUFF_SIZE /
			IPA_DMA_TEST_ASYNC_PARALLEL_LOOP_NUM),
			ipa_test_dma_async_memcpy_cb_user_data, &udata[i]);
		if (rc) {
			IPA_UT_LOG("async memcpy initiation fail i=%d rc=%d\n",
				i, rc);
			is_fail = true;
		}
	}

	for (i = 0; i < IPA_DMA_TEST_ASYNC_PARALLEL_LOOP_NUM ; i++)
		wait_for_completion(&udata[i].copy_done);

	ipa_test_dma_destroy_buffs(&all_src_mem, &all_dest_mem);
	kfree(udata);
	rc = ipa_dma_disable();
	if (rc) {
		IPA_UT_LOG("DMA disable failed rc=%d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("fail disable dma");
		return rc;
	}

	if (is_fail) {
		IPA_UT_LOG("async memcopy failed\n");
		IPA_UT_TEST_FAIL_REPORT("async memcopy failed");
		return -EFAULT;
	}

	return 0;
}

/**
 * TEST: Sync memory copy
 *
 *	1. dma enable
 *	2. sync memcpy with max packet size
 *	3. dma disable
 */
static int ipa_test_dma_sync_memcpy_max_pkt_size(void *priv)
{
	int rc;

	IPA_UT_LOG("Test Start\n");

	rc = ipa_dma_enable();
	if (rc) {
		IPA_UT_LOG("DMA enable failed rc=%d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("fail enable dma");
		return rc;
	}

	rc = ipa_test_dma_memcpy_sync(IPA_TEST_DMA_MAX_PKT_SIZE, false);
	if (rc) {
		IPA_UT_LOG("sync memcpy failed rc=%d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("sync memcpy failed");
		(void)ipa_dma_disable();
		return rc;
	}

	rc = ipa_dma_disable();
	if (rc) {
		IPA_UT_LOG("DMA disable failed rc=%d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("fail disable dma");
		return rc;
	}

	return 0;
}

/* Suite definition block */
IPA_UT_DEFINE_SUITE_START(dma, "DMA for GSI",
	ipa_test_dma_setup, ipa_test_dma_teardown)
{
	IPA_UT_ADD_TEST(init_enable_disable_destroy,
		"Init->Enable->Disable->Destroy",
		ipa_test_dma_enable_disable,
		true, IPA_HW_v3_0, IPA_HW_MAX),
	IPA_UT_ADD_TEST(initx2_enable_disable_destroyx2,
		"Initx2->Enable->Disable->Destroyx2",
		ipa_test_dma_init_enbl_disable_destroy,
		true, IPA_HW_v3_0, IPA_HW_MAX),
	IPA_UT_ADD_TEST(init_enablex2_disablex2_destroy,
		"Init->Enablex2->Disablex2->Destroy",
		ipa_test_dma_enblx2_disablex2,
		true, IPA_HW_v3_0, IPA_HW_MAX),
	IPA_UT_ADD_TEST(memcpy_before_enable,
		"Call memcpy before dma enable and expect it to fail",
		ipa_test_dma_memcpy_before_enable,
		true, IPA_HW_v3_0, IPA_HW_MAX),
	IPA_UT_ADD_TEST(sync_memcpy,
		"Sync memory copy",
		ipa_test_dma_sync_memcpy,
		true, IPA_HW_v3_0, IPA_HW_MAX),
	IPA_UT_ADD_TEST(sync_memcpy_small,
		"Small Sync memory copy",
		ipa_test_dma_sync_memcpy_small,
		true, IPA_HW_v3_5, IPA_HW_MAX),
	IPA_UT_ADD_TEST(async_memcpy,
		"Async memory copy",
		ipa_test_dma_async_memcpy,
		true, IPA_HW_v3_0, IPA_HW_MAX),
	IPA_UT_ADD_TEST(async_memcpy_small,
		"Small async memory copy",
		ipa_test_dma_async_memcpy_small,
		true, IPA_HW_v3_5, IPA_HW_MAX),
	IPA_UT_ADD_TEST(sync_memcpy_in_loop,
		"Several sync memory copy iterations",
		ipa_test_dma_sync_memcpy_in_loop,
		true, IPA_HW_v3_0, IPA_HW_MAX),
	IPA_UT_ADD_TEST(async_memcpy_in_loop,
		"Several async memory copy iterations",
		ipa_test_dma_async_memcpy_in_loop,
		true, IPA_HW_v3_0, IPA_HW_MAX),
	IPA_UT_ADD_TEST(interleaved_sync_async_memcpy_in_loop,
		"Several interleaved sync and async memory copy iterations",
		ipa_test_dma_interleaved_sync_async_memcpy_in_loop,
		true, IPA_HW_v3_0, IPA_HW_MAX),
	IPA_UT_ADD_TEST(multi_threaded_multiple_sync_async_memcpy,
		"Several multi-threaded sync and async memory copy iterations",
		ipa_test_dma_mt_sync_async,
		true, IPA_HW_v3_0, IPA_HW_MAX),
	IPA_UT_ADD_TEST(parallel_async_memcpy_in_loop,
		"Several parallel async memory copy iterations",
		ipa_test_dma_parallel_async_memcpy_in_loop,
		true, IPA_HW_v3_0, IPA_HW_MAX),
	IPA_UT_ADD_TEST(sync_memcpy_max_pkt_size,
		"Sync memory copy with max packet size",
		ipa_test_dma_sync_memcpy_max_pkt_size,
		true, IPA_HW_v3_0, IPA_HW_MAX),
} IPA_UT_DEFINE_SUITE_END(dma);
