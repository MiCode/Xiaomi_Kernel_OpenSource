// SPDX-License-Identifier: GPL-2.0

/*
 * Copyright (c) 2019 MediaTek Inc.
 */

/*
 * GenieZone (hypervisor-based seucrity platform) enables hardware protected
 * and isolated security execution environment, includes
 * 1. GZ hypervisor
 * 2. Hypervisor-TEE OS (built-in Trusty OS)
 * 3. Drivers (ex: debug, communication and interrupt) for GZ and
 *    hypervisor-TEE OS
 * 4. GZ and hypervisor-TEE and GZ framework (supporting multiple TEE
 *    ecosystem, ex: M-TEE, Trusty, GlobalPlatform, ...)
 */

#include <linux/io.h>
#include <linux/kthread.h>
#include <linux/printk.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/platform_device.h>

#include <gz-trusty/smcall.h>
#include <gz-trusty/trusty.h>

#include <tz_cross/trustzone.h>
#include <tz_cross/ta_system.h>

#include "gz_rkp_ut.h"

#define KREE_DEBUG(fmt...) pr_info("[RKP_UT]" fmt)
#define KREE_INFO(fmt...) pr_info("[RKP_UT]" fmt)
#define KREE_ERR(fmt...) pr_info("[RKP_UT][ERR]" fmt)

#define echo_6 1
#define echo_7 1
#define echo_8 1
#define echo_9 1
#define echo_a 1
#define echo_b 1
#define echo_c 1

#if echo_6
/*call RKP basic UT*/
int test_rkp_basic_by_smc(void *args)
{
	if (!tz_system_dev->dev.parent)
		return TZ_RESULT_ERROR_NO_DATA;

	/* test smc */
	trusty_std_call32(tz_system_dev->dev.parent,
		MTEE_SMCNR(MT_SMCF_FC_HEAP_DUMP, tz_system_dev->dev.parent),
		0x1, 0x1, 0x0);

	return TZ_RESULT_SUCCESS;
}

#endif

#if echo_7
/*call RKP stress UT*/
int test_rkp_stress_by_smc_body(void *args)
{
	if (!tz_system_dev->dev.parent)
		return TZ_RESULT_ERROR_NO_DATA;

	/* test smc */
	trusty_std_call32(tz_system_dev->dev.parent,
		MTEE_SMCNR(MT_SMCF_FC_HEAP_DUMP, tz_system_dev->dev.parent),
		0x2, 0x2, 0x0);

	return TZ_RESULT_SUCCESS;
}

int test_rkp_stress_by_smc(void *args)
{
	struct task_struct *thread1, *thread2, *thread3;

	thread1 = kthread_create(test_rkp_stress_by_smc_body, NULL, "test_rkp_t1");
	if (IS_ERR(thread1))
		return TZ_RESULT_ERROR_GENERIC;
	kthread_bind(thread1, 0);
	wake_up_process(thread1);

	/*thread 2: write 1000 times*/
	thread2 = kthread_create(test_rkp_stress_by_smc_body, NULL, "test_rkp_t2");
	if (IS_ERR(thread2))
		return TZ_RESULT_ERROR_GENERIC;
	kthread_bind(thread2, 5);
	wake_up_process(thread2);

	thread3 = kthread_create(test_rkp_stress_by_smc_body, NULL, "test_rkp_t3");
	if (IS_ERR(thread3))
		return TZ_RESULT_ERROR_GENERIC;
	kthread_bind(thread3, 3);
	wake_up_process(thread3);

	return 1;
}
#endif

#if echo_8

/*Test: test changing mmu type/read/write memory concurrently
 * (1) alloc a buffer (buf) with size 4KB and update
 *     ipa type to (RW +EXEC).
 * (2) use three threads concurrently:
 *     one: changing mmu type; one: reading the buffer; one: write the buffer
 */

#define test_thread_num 3
struct completion _comp[test_thread_num];

char *buf_rkp;
uint32_t pa_high, pa_low;
uint32_t buf_size = 4096; /*4KB*/

DEFINE_MUTEX(mutex_8);
int _init_test_buf(void)
{
	uint64_t pa = 0;
	int ret = TZ_RESULT_SUCCESS;
	int locktry;

	do {
		locktry = mutex_lock_interruptible(&mutex_8);
		if (locktry && locktry != -EINTR) {
			KREE_ERR("mutex_c fail(0x%x)\n", locktry);
			return TZ_RESULT_ERROR_GENERIC;
		}
	} while (locktry);

	/*alloc test buffer*/
	buf_rkp = kmalloc(buf_size, GFP_KERNEL);
	if (!buf_rkp) {
		KREE_ERR("[%s]buf_rkp kmalloc Fail.\n", __func__);
		ret = TZ_RESULT_ERROR_OUT_OF_MEMORY;
		goto out;
	}

	/*memset(buf_rkp, 'a', size);*/	/*will data abort*/

	pa = (uint64_t) virt_to_phys((void *)buf_rkp);
	KREE_INFO("[%s]test buf_rkp pa:0x%llx\n", __func__, pa);

	pa_high = (uint32_t) (pa >> 32);
	pa_low = (uint32_t) (pa & (0x00000000ffffffff));
	KREE_INFO("[%s][%d] pa=0x%llx, high=0x%x, low=0x%x\n",
		__func__, __LINE__, pa, pa_high, pa_low);

	if ((pa % PAGE_SIZE) != 0) {
		KREE_INFO("[%s][%d] pa = 0x%llx not aligned 4KB.\n",
			__func__, __LINE__, pa);
		ret = TZ_RESULT_ERROR_BAD_PARAMETERS;
		goto out;
	}

out:
	mutex_unlock(&mutex_8);

	return ret;
}

int _init_thread_completion(void)
{
	int i;

	for (i = 0; i < test_thread_num; i++)
		init_completion(&_comp[i]);

	return 0;
}

int _wait_thread_completion(void)
{
	int i;

	for (i = 0; i < test_thread_num; i++)
		wait_for_completion(&_comp[i]);

	return 0;
}

int _thread_change_type(void *data)
{
	/* test smc */
	int i;

	if (!tz_system_dev->dev.parent)
		return TZ_RESULT_ERROR_NO_DATA;

	for (i = 0; i < 100; i++) {
		trusty_std_call32(tz_system_dev->dev.parent,
			MTEE_SMCNR(MT_SMCF_FC_HEAP_DUMP, tz_system_dev->dev.parent),
			pa_high, pa_low, buf_size);
	}
	complete(&_comp[0]);
	return TZ_RESULT_SUCCESS;
}

int _thread_write(void *data)
{
	int i;
	for (i = 0; i < 1000; i++) {
		if (buf_rkp) {
			buf_rkp[10] = 'c';
			//memset(buf_rkp, 'c', 4096);
		}
	}
	complete(&_comp[1]);
	return 0;
}

int _thread_read(void *data)
{
	int i, j;
	char m;

	for (i = 0; i < 1000; i++)
		for (j = 0; j < 4096; j++)
			if (buf_rkp)
				m = (char) buf_rkp[j];

	complete(&_comp[2]);
	return 0;
}

/*echo 8: one-thread: chage mmu type, one-thread: read, one-thread: write*/
int test_rkp_mix_op(void *args)
{
	struct task_struct *thread_mmu, *thread_wrt, *thread_rd;
	int ret = 0x0;

	ret = _init_test_buf();
	if ((!buf_rkp) || (ret != TZ_RESULT_SUCCESS))
		goto out;

	KREE_INFO("[%s][%d] high=0x%x, low=0x%x\n",
		__func__, __LINE__, pa_high, pa_low);

	_init_thread_completion();

	/*thread 1: do map/unmap 1000 times*/
	thread_mmu = kthread_create(_thread_change_type, NULL, "T_1");
	if (IS_ERR(thread_mmu))
		return TZ_RESULT_ERROR_GENERIC;
	kthread_bind(thread_mmu, 3);
	wake_up_process(thread_mmu);

	/*thread 2: write 1000 times*/
	thread_wrt = kthread_create(_thread_write, NULL, "T_W");
	if (IS_ERR(thread_wrt))
		return TZ_RESULT_ERROR_GENERIC;

	kthread_bind(thread_wrt, 4);
	wake_up_process(thread_wrt);

	/*thread 3: read 1000 times*/
	thread_rd = kthread_create(_thread_read, NULL, "T_R");
	if (IS_ERR(thread_rd))
		return TZ_RESULT_ERROR_GENERIC;
	kthread_bind(thread_rd, 3);
	wake_up_process(thread_rd);

	_wait_thread_completion();

out:
	kfree(buf_rkp);

	return 0;
}

#endif

#if echo_9
DEFINE_MUTEX(mutex_9);

//extern struct miscdevice gz_device;
int malloc_buffer_by_smc(void *args)
{
	char *buf;
	int size = 4096; /*can update */
	uint64_t pa;
	uint32_t high, low;
	int ret = TZ_RESULT_SUCCESS;
	int locktry;

	if (!tz_system_dev->dev.parent)
		return TZ_RESULT_ERROR_NO_DATA;

	do {
		locktry = mutex_lock_interruptible(&mutex_9);
		if (locktry && locktry != -EINTR) {
			KREE_ERR("mutex_c fail(0x%x)\n", locktry);
			return TZ_RESULT_ERROR_GENERIC;
		}
	} while (locktry);

	buf = kmalloc(size, GFP_KERNEL);
	if (!buf) {
		KREE_INFO("[%s] buf kmalloc Fail.\n", __func__);
		ret = TZ_RESULT_ERROR_OUT_OF_MEMORY;
		goto out;
	}

	memset(buf, 'a', size);	/*init data */

	pa = (uint64_t) virt_to_phys((void *)buf);
	high = (uint32_t) ((uint64_t) pa >> 32);
	low = (uint32_t) ((uint64_t) pa & (0x00000000ffffffff));

	KREE_INFO("[%s][%d] pa=0x%llx, high=0x%x, low=0x%x\n",
		__func__, __LINE__, pa, high, low);

	if ((pa % PAGE_SIZE) != 0) {
		KREE_INFO("[%s][%d] pa = 0x%llx not aligned 4KB.\n",
			__func__, __LINE__, pa);
		ret = TZ_RESULT_ERROR_BAD_PARAMETERS;
		goto out;
	}

	/* test smc */
	trusty_std_call32(tz_system_dev->dev.parent,
		MTEE_SMCNR(MT_SMCF_FC_HEAP_DUMP, tz_system_dev->dev.parent),
		high, low, size);
out:
	kfree(buf);
	mutex_unlock(&mutex_9);
	return ret;
}

/*echo 9:test RKP UT by allocated buffer in CA*/
int test_rkp_by_malloc_buf(void *args)
{
/*fix me. _text, _etext cannot use in .ko. disable UT first*/

//	uint64_t size = (uint64_t)_etext - (uint64_t)_text;
//	uint64_t pa = (uint64_t) virt_to_phys((void *)_text);

//	KREE_INFO("[%s][%d] test param: _etext=0x%llx, _text=0x%llx,",
//		__func__, __LINE__, (uint64_t)_etext, (uint64_t)_text);
//	KREE_INFO("start_pa=0x%llx, size=0x%llx\n", pa, size);

//	return _thread_mmu_gzdriver_body((uint64_t)pa, (uint64_t)size,
//		0x2); /*RO, EXECUTE*/

	struct task_struct *thread1, *thread2;

	thread1 = kthread_create(malloc_buffer_by_smc, NULL, "test_rkp_t1");
	if (IS_ERR(thread1))
		return TZ_RESULT_ERROR_GENERIC;
	kthread_bind(thread1, 0);
	wake_up_process(thread1);

	thread2 = kthread_create(malloc_buffer_by_smc, NULL, "test_rkp_t2");
	if (IS_ERR(thread2))
		return TZ_RESULT_ERROR_GENERIC;
	kthread_bind(thread2, 5);
	wake_up_process(thread2);

	return TZ_RESULT_SUCCESS;
}
#endif


#if echo_a

extern int64_t va_gz_test_store;
DEFINE_MUTEX(mutex_a);

int test_rkp_by_gz_driver_body(void)
{
	int size = 4096; /*can update */
	uint64_t pa;
	uint32_t high, low;

	int64_t test_va = 0x0;
	int64_t test_pa = 0x0;
	int64_t align_test_pa = 0x0;

	if (!tz_system_dev->dev.parent)
		return TZ_RESULT_ERROR_NO_DATA;

	test_va = va_gz_test_store;
	test_pa = (uint64_t) virt_to_phys((void *)test_va);
	align_test_pa = test_pa & 0xfffffffffffff000;

	pa = (uint64_t) align_test_pa;
	high = (uint32_t) ((uint64_t) pa >> 32);
	low = (uint32_t) ((uint64_t) pa & (0x00000000ffffffff));

	KREE_INFO("[%s][%d] pa=0x%llx, high=0x%x, low=0x%x\n",
		__func__, __LINE__, pa, high, low);

	if ((pa % PAGE_SIZE) != 0) {
		KREE_INFO("[%s][%d] pa = 0x%llx not aligned 4KB.\n",
			__func__, __LINE__, pa);
		goto out;
	}

	/* test smc */
	trusty_std_call32(tz_system_dev->dev.parent,
		MTEE_SMCNR(MT_SMCF_FC_HEAP_DUMP, tz_system_dev->dev.parent),
		high, low, size);
out:
	return TZ_RESULT_SUCCESS;
}

int test_rkp_by_gz_driver(void *args)
{
	int ret = TZ_RESULT_SUCCESS;
	int locktry;

	do {
		locktry = mutex_lock_interruptible(&mutex_a);
		if (locktry && locktry != -EINTR) {
			KREE_ERR("mutex_c fail(0x%x)\n", locktry);
			return TZ_RESULT_ERROR_GENERIC;
		}
	} while (locktry);

	test_rkp_by_gz_driver_body();

	mutex_unlock(&mutex_a);

	return ret;
}
#endif

#if echo_b

/*vreg: the same setting in gz*/
#define test_vreg_src_pa  0x1070A000  /*can modify*/
#define test_vreg_basic_size  0x1000  /*can modify*/

#define VREG_BASE_dyn_map (test_vreg_src_pa + 1 * test_vreg_basic_size)

#define WORD_WIDTH 4 /*32-bit*/
DEFINE_MUTEX(mutex_b);
/*call RKP basic UT*/
int test_rkp_basic_by_vreg(void *args)
{
	void __iomem *io;
	int ret = TZ_RESULT_SUCCESS;
	int locktry;

	do {
		locktry = mutex_lock_interruptible(&mutex_b);
		if (locktry && locktry != -EINTR) {
			KREE_ERR("mutex_c fail(0x%x)\n", locktry);
			return TZ_RESULT_ERROR_GENERIC;
		}
	} while (locktry);

	io = ioremap(VREG_BASE_dyn_map, test_vreg_basic_size);
	if (!io) {
		ret = TZ_RESULT_ERROR_GENERIC;
		goto out;
	}

	writel(0x00000001, io + (0 * WORD_WIDTH));

	if (io)
		iounmap(io);

out:
	mutex_unlock(&mutex_b);

	return ret;
}

#endif


#if echo_c
DEFINE_MUTEX(mutex_c);
/*call RKP stress UT*/
int call_rkp_stressUT_by_vreg(void *args)
{
	void __iomem *io;
	uint32_t v;
	int ret = TZ_RESULT_SUCCESS;
	int locktry;

	do {
		locktry = mutex_lock_interruptible(&mutex_c);
		if (locktry && locktry != -EINTR) {
			KREE_ERR("mutex_c fail(0x%x)\n", locktry);
			return TZ_RESULT_ERROR_GENERIC;
		}
	} while (locktry);

	io = ioremap(VREG_BASE_dyn_map, test_vreg_basic_size);
	if (!io) {
		ret = TZ_RESULT_ERROR_GENERIC;
		goto out;
	}

	writel(0x00000002, io + (0 * WORD_WIDTH));

	v = readl(io + (0 * WORD_WIDTH));
	KREE_INFO("[%s] read [%d]=0x%x\n",
		__func__, (io + (0 * WORD_WIDTH)), v);

	if (io)
		iounmap(io);

out:
	mutex_unlock(&mutex_c);

	return ret;
}

int test_rkp_stress_by_vreg(void *args)
{
	struct task_struct *thread1, *thread2, *thread3;

	thread1 = kthread_create(call_rkp_stressUT_by_vreg, NULL, "test_rkp_t1");
	if (IS_ERR(thread1))
		return TZ_RESULT_ERROR_GENERIC;
	kthread_bind(thread1, 0);
	wake_up_process(thread1);

	/*thread 2: write 1000 times*/
	thread2 = kthread_create(call_rkp_stressUT_by_vreg, NULL, "test_rkp_t2");
	if (IS_ERR(thread2))
		return TZ_RESULT_ERROR_GENERIC;
	kthread_bind(thread2, 5);
	wake_up_process(thread2);

	thread3 = kthread_create(call_rkp_stressUT_by_vreg, NULL, "test_rkp_t3");
	if (IS_ERR(thread3))
		return TZ_RESULT_ERROR_GENERIC;
	kthread_bind(thread3, 3);
	wake_up_process(thread3);

	return TZ_RESULT_SUCCESS;
}
#endif
