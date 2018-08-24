/* Copyright (c) 2018, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include "hab.h"
#if !defined CONFIG_GHS_VMM && defined(CONFIG_MSM_GVM_QUIN)
#include <asm/cacheflush.h>
#include <linux/list.h>
#include "hab_pipe.h"
#include "hab_qvm.h"
#include "khab_test.h"

static char g_perf_test_result[256];

enum hab_perf_test_type {
	HAB_SHMM_THGPUT = 0x0,
};

#define HAB_PERF_TEST_MMID 802
#define PERF_TEST_ITERATION 50
#define MEM_READ_ITERATION 30

static int hab_shmm_throughput_test(void)
{
	struct hab_device *habDev;
	struct qvm_channel *dev;
	struct hab_shared_buf *sh_buf;
	struct physical_channel *pchan;
	struct timeval tv1, tv2;
	int i, counter;
	void *test_data;
	unsigned char *source_data, *shmm_adr;

	register int sum;
	register int *pp, *lastone;
	int throughput[3][2] = { {0} };
	int latency[6][PERF_TEST_ITERATION];
	int ret = 0, tmp, size;

	habDev = find_hab_device(HAB_PERF_TEST_MMID);
	if (!habDev || list_empty(&(habDev->pchannels))) {
		ret = -ENOMEM;
		return ret;
	}

	pchan = list_first_entry(&(habDev->pchannels),
		struct physical_channel, node);
	dev = pchan->hyp_data;
	if (!dev) {
		ret = -EPERM;
		return ret;
	}

	sh_buf = dev->pipe_ep->tx_info.sh_buf;

	/* pChannel is of 128k, we use 64k to test */
	size = 0x10000;

	if (!sh_buf) {
		pr_err("Share buffer address is empty, exit the perf test\n");
		ret = -ENOMEM;
		return ret;
	}
	shmm_adr = sh_buf->data;

	test_data = kzalloc(size, GFP_ATOMIC);
	if (!test_data) {
		ret = -ENOMEM;
		return ret;
	}

	source_data = kzalloc(size, GFP_ATOMIC);
	if (!source_data) {
		ret = -ENOMEM;
		return ret;
	}

	for (i = 0; i < PERF_TEST_ITERATION; i++) {
		/* Normal memory copy latency */
		flush_cache_all();
		do_gettimeofday(&tv1);
		memcpy(test_data, source_data, size);
		do_gettimeofday(&tv2);
		latency[0][i] = (tv2.tv_sec - tv1.tv_sec)*1000000
			+ (tv2.tv_usec - tv1.tv_usec);

		/* Share memory copy latency */
		flush_cache_all();
		do_gettimeofday(&tv1);
		memcpy(shmm_adr, source_data, size);
		do_gettimeofday(&tv2);
		latency[1][i] = (tv2.tv_sec - tv1.tv_sec)*1000000
			+ (tv2.tv_usec - tv1.tv_usec);

		/* Normal memory read latency */
		counter = MEM_READ_ITERATION;
		sum = 0;
		latency[2][i] = 0;
		flush_cache_all();
		while (counter-- > 0) {
			pp = test_data;
			lastone = (int *)((char *)test_data + size - 512);
			do_gettimeofday(&tv1);
			while (pp <= lastone) {
				sum +=
				pp[0] + pp[4] + pp[8] + pp[12]
				+ pp[16] + pp[20] + pp[24] + pp[28]
				+ pp[32] + pp[36] + pp[40] + pp[44]
				+ pp[48] + pp[52] + pp[56] + pp[60]
				+ pp[64] + pp[68] + pp[72] + pp[76]
				+ pp[80] + pp[84] + pp[88] + pp[92]
				+ pp[96] + pp[100] + pp[104]
				+ pp[108] + pp[112]
				+ pp[116] + pp[120]
				+ pp[124];
				pp +=  128;
			}
			do_gettimeofday(&tv2);
			latency[2][i] += (tv2.tv_sec - tv1.tv_sec)*1000000
				+ (tv2.tv_usec - tv1.tv_usec);
			flush_cache_all();
		}

		/* Share memory read latency*/
		counter = MEM_READ_ITERATION;
		sum = 0;
		latency[3][i] = 0;
		while (counter-- > 0) {
			pp = (int *)shmm_adr;
			lastone = (int *)(shmm_adr + size - 512);
			do_gettimeofday(&tv1);
			while (pp <= lastone) {
				sum +=
				pp[0] + pp[4] + pp[8] + pp[12]
				+ pp[16] + pp[20] + pp[24] + pp[28]
				+ pp[32] + pp[36] + pp[40] + pp[44]
				+ pp[48] + pp[52] + pp[56] + pp[60]
				+ pp[64] + pp[68] + pp[72] + pp[76]
				+ pp[80] + pp[84] + pp[88] + pp[92]
				+ pp[96] + pp[100] + pp[104]
				+ pp[108] + pp[112]
				+ pp[116] + pp[120]
				+ pp[124];
				pp +=  128;
			}
			do_gettimeofday(&tv2);
			latency[3][i] += (tv2.tv_sec - tv1.tv_sec)*1000000
				+ (tv2.tv_usec - tv1.tv_usec);
			flush_cache_all();
		}

		/* Normal memory write latency */
		flush_cache_all();
		do_gettimeofday(&tv1);
		memset(test_data, 'c', size);
		do_gettimeofday(&tv2);
		latency[4][i] = (tv2.tv_sec - tv1.tv_sec)*1000000
			+ (tv2.tv_usec - tv1.tv_usec);

		/* Share memory write latency */
		flush_cache_all();
		do_gettimeofday(&tv1);
		memset(shmm_adr, 'c', size);
		do_gettimeofday(&tv2);
		latency[5][i] = (tv2.tv_sec - tv1.tv_sec)*1000000
			+ (tv2.tv_usec - tv1.tv_usec);
	}

	/* Calculate normal memory copy throughput by average */
	tmp = 0;
	for (i = 0; i < PERF_TEST_ITERATION; i++)
		tmp += latency[0][i];
	throughput[0][0] = (tmp != 0) ? size*PERF_TEST_ITERATION/tmp : 0;

	/* Calculate share memory copy throughput by average */
	tmp = 0;
	for (i = 0; i < PERF_TEST_ITERATION; i++)
		tmp += latency[1][i];
	throughput[0][1] = (tmp != 0) ? size*PERF_TEST_ITERATION/tmp : 0;

	/* Calculate normal memory read throughput by average */
	tmp = 0;
	for (i = 0; i < PERF_TEST_ITERATION; i++)
		tmp += latency[2][i];
	throughput[1][0] = (tmp != 0) ?
		size*PERF_TEST_ITERATION*MEM_READ_ITERATION/tmp : 0;

	/* Calculate share memory read throughput by average */
	tmp = 0;
	for (i = 0; i < PERF_TEST_ITERATION; i++)
		tmp += latency[3][i];
	throughput[1][1] = (tmp != 0) ?
		size*PERF_TEST_ITERATION*MEM_READ_ITERATION/tmp : 0;

	/* Calculate normal memory write throughput by average */
	tmp = 0;
	for (i = 0; i < PERF_TEST_ITERATION; i++)
		tmp += latency[4][i];
	throughput[2][0] = (tmp != 0) ?
		size*PERF_TEST_ITERATION/tmp : 0;

	/* Calculate share memory write throughput by average */
	tmp = 0;
	for (i = 0; i < PERF_TEST_ITERATION; i++)
		tmp += latency[5][i];
	throughput[2][1] = (tmp != 0) ?
		size*PERF_TEST_ITERATION/tmp : 0;

	kfree(test_data);
	kfree(source_data);

	snprintf(g_perf_test_result, sizeof(g_perf_test_result),
		"cpy(%d,%d)/read(%d,%d)/write(%d,%d)",
		throughput[0][0], throughput[0][1], throughput[1][0],
		throughput[1][1], throughput[2][0], throughput[2][1]);

	return ret;
}

int hab_perf_test(long testId)
{
	int ret;

	switch (testId) {
	case HAB_SHMM_THGPUT:
		ret = hab_shmm_throughput_test();
		break;
	default:
		pr_err("Invalid performance test ID %ld\n", testId);
		ret = -EINVAL;
	}

	return ret;
}

static int kick_hab_perf_test(const char *val, struct kernel_param *kp);
static int get_hab_perf_result(char *buffer, struct kernel_param *kp);

module_param_call(perf_test, kick_hab_perf_test, get_hab_perf_result,
		  NULL, S_IRUSR | S_IWUSR);

static int kick_hab_perf_test(const char *val, struct kernel_param *kp)
{
	long testId;
	int err = kstrtol(val, 10, &testId);

	if (err)
		return err;
	memset(g_perf_test_result, 0, sizeof(g_perf_test_result));
	return hab_perf_test(testId);
}

static int get_hab_perf_result(char *buffer, struct kernel_param *kp)
{
	return strlcpy(buffer, g_perf_test_result,
		strlen(g_perf_test_result)+1);
}
#endif

static struct kobject *hab_kobject;

static int vchan_stat;
static int context_stat;
static int pid_stat;

static ssize_t vchan_show(struct kobject *kobj, struct kobj_attribute *attr,
						char *buf)
{
	return hab_stat_show_vchan(&hab_driver, buf, PAGE_SIZE);
}

static ssize_t vchan_store(struct kobject *kobj, struct kobj_attribute *attr,
						const char *buf, size_t count)
{
	int ret;

	ret = sscanf(buf, "%du", &vchan_stat);
	if (ret < 1) {
		pr_err("failed to read anything from input %d", ret);
		return 0;
	} else
		return vchan_stat;
}

static ssize_t ctx_show(struct kobject *kobj, struct kobj_attribute *attr,
						char *buf)
{
	return hab_stat_show_ctx(&hab_driver, buf, PAGE_SIZE);
}

static ssize_t ctx_store(struct kobject *kobj, struct kobj_attribute *attr,
						const char *buf, size_t count)
{
	int ret;

	ret = sscanf(buf, "%du", &context_stat);
	if (ret < 1) {
		pr_err("failed to read anything from input %d", ret);
		return 0;
	} else
		return context_stat;
}

static ssize_t expimp_show(struct kobject *kobj, struct kobj_attribute *attr,
						char *buf)
{
	return hab_stat_show_expimp(&hab_driver, pid_stat, buf, PAGE_SIZE);
}

static ssize_t expimp_store(struct kobject *kobj, struct kobj_attribute *attr,
						const char *buf, size_t count)
{
	int ret;

	ret = sscanf(buf, "%du", &pid_stat);
	if (ret < 1) {
		pr_err("failed to read anything from input %d", ret);
		return 0;
	} else
		return pid_stat;
}

static struct kobj_attribute vchan_attribute = __ATTR(vchan_stat, 0660,
								vchan_show,
								vchan_store);

static struct kobj_attribute ctx_attribute = __ATTR(context_stat, 0660,
								ctx_show,
								ctx_store);

static struct kobj_attribute expimp_attribute = __ATTR(pid_stat, 0660,
								expimp_show,
								expimp_store);

int hab_stat_init_sub(struct hab_driver *driver)
{
	int result;

	hab_kobject = kobject_create_and_add("hab", kernel_kobj);
	if (!hab_kobject)
		return -ENOMEM;

	result = sysfs_create_file(hab_kobject, &vchan_attribute.attr);
	if (result)
		pr_debug("cannot add vchan in /sys/kernel/hab %d\n", result);

	result = sysfs_create_file(hab_kobject, &ctx_attribute.attr);
	if (result)
		pr_debug("cannot add ctx in /sys/kernel/hab %d\n", result);

	result = sysfs_create_file(hab_kobject, &expimp_attribute.attr);
	if (result)
		pr_debug("cannot add expimp in /sys/kernel/hab %d\n", result);

	return result;
}

int hab_stat_deinit_sub(struct hab_driver *driver)
{
	sysfs_remove_file(hab_kobject, &vchan_attribute.attr);
	sysfs_remove_file(hab_kobject, &ctx_attribute.attr);
	sysfs_remove_file(hab_kobject, &expimp_attribute.attr);
	kobject_put(hab_kobject);

	return 0;
}
