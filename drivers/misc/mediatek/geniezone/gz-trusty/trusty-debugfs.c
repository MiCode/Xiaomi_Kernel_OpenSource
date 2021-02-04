/*
 * Copyright (C) 2019 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include <linux/random.h>
#include <gz-trusty/trusty.h>
#include <gz-trusty/smcall.h>
#include <linux/kthread.h>
#include <linux/signal.h>

/*** Trusty MT test device attributes ***/
static struct task_struct *trusty_task;
static struct task_struct *nebula_task;
static struct device *mtee_dev[TEE_ID_END];
static int cpu[TEE_ID_END] = { 0 };
static uint64_t s_cnt[TEE_ID_END] = { 0 }, f_cnt[TEE_ID_END] = { 0 };

static int stress_trusty_thread(void *data)
{
	s32 a, b, ret;
	struct device *dev = mtee_dev[TEE_ID_TRUSTY];

	s_cnt[0] = f_cnt[0] = 0;

	allow_signal(SIGKILL);

	while (!kthread_should_stop()) {
		get_random_bytes(&a, sizeof(s32));
		a &= 0xDFFFFFFF;
		get_random_bytes(&b, sizeof(s32));
		b &= 0xDFFFFFF;
		ret = trusty_std_call32(dev,
					MTEE_SMCNR(MT_SMCF_SC_ADD, dev),
					a, b, 0);

		if ((a + b) == ret)
			s_cnt[0]++;
		else
			f_cnt[0]++;

		pr_info_ratelimited("[%lld/%lld] %u + %u = %u, %s\n",
				s_cnt[0], f_cnt[0], a, b, ret,
				(a + b) == ret ? "PASS" : "FAIL");

		if (signal_pending(trusty_task))
			break;
	}

	pr_debug("[%s] End of test, succeed %lld, failed %lld\n",
		 __func__, s_cnt[0], f_cnt[0]);

	s_cnt[0] = f_cnt[0] = 0;
	return 0;
}

static int stress_nebula_thread(void *data)
{
	s32 a, b, c, ret;
	struct device *dev = mtee_dev[TEE_ID_NEBULA];

	s_cnt[1] = f_cnt[1] = 0;

	allow_signal(SIGKILL);

	while (!kthread_should_stop()) {
		get_random_bytes(&a, sizeof(s32));
		a &= 0xFF;
		get_random_bytes(&b, sizeof(s32));
		b &= 0xFF;
		get_random_bytes(&c, sizeof(s32));
		c &= 0xFF;
		ret = trusty_std_call32(dev,
				MTEE_SMCNR(SMCF_SC_TEST_MULTIPLY, dev),
				a, b, c);

		if ((a * b * c) == ret)
			s_cnt[1]++;
		else
			f_cnt[1]++;

		pr_info_ratelimited("[%lld/%lld] %d * %d * %d= %d, %s\n",
				s_cnt[1], f_cnt[1], a, b, c,
				ret, (a * b * c) == ret ? "PASS" : "FAIL");

		if (signal_pending(nebula_task))
			break;
	}

	pr_info("[%s] End of test, succeed %lld, failed %lld\n",
		__func__, s_cnt[1], f_cnt[1]);

	s_cnt[1] = f_cnt[1] = 0;
	return 0;
}

static ssize_t gz_concurrent_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	char str[256], tmp[256];
	int i = 0;

	str[0] = '\0';

	if (trusty_task) {
		i = snprintf(tmp, 256,
			"stress_trusty on CPU %d, succeed %lld, failed %lld\n",
			cpu[0], s_cnt[0], f_cnt[0]);
		tmp[i] = '\0';
		strncat(str, tmp, 256);
		strncat(str, "MTEE 1.0 :stress_trusty_thread Running\n", 256);
		pr_info("stress_trusty on CPU %d, succeed %lld, failed %lld\n",
			cpu[0], s_cnt[0], f_cnt[0]);
	}

	if (nebula_task) {
		i = snprintf(tmp, 256,
			"stress_nebula on CPU %d, succeed %lld, failed %lld\n",
			cpu[1], s_cnt[1], f_cnt[1]);
		tmp[i] = '\0';
		strncat(str, tmp, 256);
		strncat(str, "MTEE 2.0 :stress_nebula_thread Running\n", 256);
		pr_info("stress_nebula on CPU %d, succeed %lld, failed %lld\n",
			cpu[1], s_cnt[1], f_cnt[1]);
	}

	if (!trusty_task && !nebula_task) {
		strncat(str,
			"Usage:\techo 1 > start default smc stress for gz33\n",
			256);
		strncat(str, "\techo 55 > both thread on cpu 5\n", 256);
		strncat(str,
			"\techo 56 > one thread on cpu 5, another on cpu 6\n",
			256);
		strncat(str, "\techo 0 > to stop\n", 256);
	}
	return scnprintf(buf, PAGE_SIZE, "%s", str);
}

static ssize_t gz_concurrent_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t n)
{
	unsigned long tmp;

	if (!buf)
		return -EINVAL;

	if (kstrtoul(buf, 10, &tmp)) {
		pr_info("[%s] convert to number failed\n", __func__);
		return -1;
	}

	tmp %= 100;
	pr_info("[%s] get number %d\n", __func__, tmp);

	if (tmp == 0) {
		if (trusty_task) {
			pr_info("[%s] Stop stress_trusty_thread", __func__);
			kthread_stop(trusty_task);
		}

		if (nebula_task) {
			pr_info("[%s] Stop stress_nebula_thread", __func__);
			kthread_stop(nebula_task);
		}
		trusty_task = nebula_task = NULL;
		return n;
	}

	if (trusty_task || nebula_task) {
		pr_info("[%s] Start already!\n", __func__);
		return n;
	}

	trusty_task = kthread_create(stress_trusty_thread, dev,
				"stress_trusty_thread");

	nebula_task = kthread_create(stress_nebula_thread, dev,
				"stress_nebula_thread");

	cpu[0] = cpu[1] = 0;

	if (tmp == 1) {
		cpu[0] = 5;
		cpu[1] = 6;
	} else {
		cpu[0] = (tmp / 10) & (num_possible_cpus() - 1);
		cpu[1] = (tmp % 10) & (num_possible_cpus() - 1);
	}

	kthread_bind(trusty_task, cpu[0]);
	if (IS_ERR(trusty_task))
		pr_info("[%s] Unable to start on cpu %d: stress_trusty_thread",
			__func__, cpu[0]);
	else {
		pr_info("[%s] Start stress_trusty_thread on cpu %d",
			__func__, cpu[0]);
		wake_up_process(trusty_task);
	}

	kthread_bind(nebula_task, cpu[1]);
	if (IS_ERR(nebula_task))
		pr_info("[%s] Unable to start at cpu %d: stress_nebula_thread",
			__func__, cpu[1]);
	else {
		pr_info("[%s] Start stress_nebula_thread on cpu %d",
			__func__, cpu[1]);
		wake_up_process(nebula_task);
	}

	return n;
}
DEVICE_ATTR_RW(gz_concurrent);

static ssize_t trusty_add_show(struct device *dev,
		   struct device_attribute *attr, char *buf)
{
	s32 a, b, ret;

	get_random_bytes(&a, sizeof(s32));
	a &= 0xFF;
	get_random_bytes(&b, sizeof(s32));
	b &= 0xFF;
	ret = trusty_std_call32(dev, MTEE_SMCNR(MT_SMCF_SC_ADD, dev),
				a, b, 0);
	return scnprintf(buf, PAGE_SIZE, "%d + %d = %d, %s\n", a, b, ret,
			 (a + b) == ret ? "PASS" : "FAIL");
}

static ssize_t trusty_add_store(struct device *dev,
			   struct device_attribute *attr, const char *buf,
			   size_t n)
{
	return n;
}
DEVICE_ATTR_RW(trusty_add);

static ssize_t trusty_threads_show(struct device *dev,
		       struct device_attribute *attr, char *buf)
{
	/* Dump Trusty threads info to memlog */
	trusty_fast_call32(dev, MTEE_SMCNR(MT_SMCF_FC_THREADS, dev), 0, 0, 0);
	/* Dump threads info from memlog to kmsg */
	trusty_std_call32(dev, MTEE_SMCNR(SMCF_SC_NOP, dev), 0, 0, 0);
	return 0;
}
static ssize_t trusty_threads_store(struct device *dev,
			   struct device_attribute *attr, const char *buf,
			   size_t n)
{
	return n;
}
DEVICE_ATTR_RW(trusty_threads);

static ssize_t trusty_threadstats_show(struct device *dev,
			   struct device_attribute *attr, char *buf)
{
	/* Dump Trusty threads info to memlog */
	trusty_fast_call32(dev, MTEE_SMCNR(MT_SMCF_FC_THREADSTATS, dev),
			   0, 0, 0);
	/* Dump threads info from memlog to kmsg */
	trusty_std_call32(dev, MTEE_SMCNR(SMCF_SC_NOP, dev), 0, 0, 0);
	return 0;
}
static ssize_t trusty_threadstats_store(struct device *dev,
			   struct device_attribute *attr, const char *buf,
			   size_t n)
{
	return n;
}
DEVICE_ATTR_RW(trusty_threadstats);

static ssize_t trusty_threadload_show(struct device *dev,
			  struct device_attribute *attr, char *buf)
{
	/* Dump Trusty threads info to memlog */
	trusty_fast_call32(dev, MTEE_SMCNR(MT_SMCF_FC_THREADLOAD, dev),
			   0, 0, 0);
	/* Dump threads info from memlog to kmsg */
	trusty_std_call32(dev, MTEE_SMCNR(SMCF_SC_NOP, dev), 0, 0, 0);
	return 0;
}
static ssize_t trusty_threadload_store(struct device *dev,
			   struct device_attribute *attr, const char *buf,
			   size_t n)
{
	return n;
}
DEVICE_ATTR_RW(trusty_threadload);

static ssize_t trusty_heap_dump_show(struct device *dev,
			 struct device_attribute *attr, char *buf)
{
	/* Dump Trusty threads info to memlog */
	trusty_fast_call32(dev, MTEE_SMCNR(MT_SMCF_FC_HEAP_DUMP, dev), 0, 0, 0);
	/* Dump threads info from memlog to kmsg */
	trusty_std_call32(dev, MTEE_SMCNR(SMCF_SC_NOP, dev), 0, 0, 0);
	return 0;
}
static ssize_t trusty_heap_dump_store(struct device *dev,
			   struct device_attribute *attr, const char *buf,
			   size_t n)
{
	return n;
}
DEVICE_ATTR_RW(trusty_heap_dump);

static ssize_t trusty_apps_show(struct device *dev,
		    struct device_attribute *attr, char *buf)
{
	/* Dump Trusty threads info to memlog */
	trusty_fast_call32(dev, MTEE_SMCNR(MT_SMCF_FC_APPS, dev), 0, 0, 0);
	/* Dump threads info from memlog to kmsg */
	trusty_std_call32(dev, MTEE_SMCNR(SMCF_SC_NOP, dev), 0, 0, 0);
	return 0;
}
static ssize_t trusty_apps_store(struct device *dev,
			   struct device_attribute *attr, const char *buf,
			   size_t n)
{
	return n;
}
DEVICE_ATTR_RW(trusty_apps);

static ssize_t trusty_vdev_reset_show(struct device *dev,
			  struct device_attribute *attr, char *buf)
{
	trusty_std_call32(dev, MTEE_SMCNR(SMCF_SC_VDEV_RESET, dev), 0, 0, 0);
	return 0;
}

static ssize_t trusty_vdev_reset_store(struct device *dev,
			   struct device_attribute *attr, const char *buf,
			   size_t n)
{
	return n;
}
DEVICE_ATTR_RW(trusty_vdev_reset);

static void trusty_create_debugfs(struct trusty_state *s, struct device *dev)
{
	int ret;

	pr_info("%s-%s\n", __func__, get_tee_name(s->tee_id));

	mtee_dev[s->tee_id] = dev;

	ret = device_create_file(dev, &dev_attr_gz_concurrent);
	if (ret)
		goto err_create_gz_concurrent;

	ret = device_create_file(dev, &dev_attr_trusty_add);
	if (ret)
		goto err_create_trusty_add;

	ret = device_create_file(dev, &dev_attr_trusty_threads);
	if (ret)
		goto err_create_trusty_threads;

	ret = device_create_file(dev, &dev_attr_trusty_threadstats);
	if (ret)
		goto err_create_trusty_threadstats;

	ret = device_create_file(dev, &dev_attr_trusty_threadload);
	if (ret)
		goto err_create_trusty_threadload;

	ret = device_create_file(dev, &dev_attr_trusty_heap_dump);
	if (ret)
		goto err_create_trusty_heap_dump;

	ret = device_create_file(dev, &dev_attr_trusty_apps);
	if (ret)
		goto err_create_trusty_apps;

	ret = device_create_file(dev, &dev_attr_trusty_vdev_reset);
	if (ret)
		goto err_create_trusty_vdev_reset;

	return;

err_create_trusty_vdev_reset:
	device_remove_file(dev, &dev_attr_trusty_vdev_reset);
err_create_trusty_apps:
	device_remove_file(dev, &dev_attr_trusty_apps);
err_create_trusty_heap_dump:
	device_remove_file(dev, &dev_attr_trusty_heap_dump);
err_create_trusty_threadload:
	device_remove_file(dev, &dev_attr_trusty_threadload);
err_create_trusty_threadstats:
	device_remove_file(dev, &dev_attr_trusty_threadstats);
err_create_trusty_threads:
	device_remove_file(dev, &dev_attr_trusty_threads);
err_create_trusty_add:
	device_remove_file(dev, &dev_attr_trusty_add);
err_create_gz_concurrent:
	device_remove_file(dev, &dev_attr_gz_concurrent);
}

/*** Nebula MT test device attributes ***/
static ssize_t vmm_fast_add_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	s32 a, b, c, ret;

	get_random_bytes(&a, sizeof(s32));
	a &= 0xFF;
	get_random_bytes(&b, sizeof(s32));
	b &= 0xFF;
	get_random_bytes(&c, sizeof(s32));
	c &= 0xFF;
	ret = trusty_fast_call32(dev, MTEE_SMCNR(SMCF_FC_TEST_ADD, dev),
				 a, b, c);
	return scnprintf(buf, PAGE_SIZE, "%d + %d + %d = %d, %s\n", a, b, c,
			 ret, (a + b + c) == ret ? "PASS" : "FAIL");
}

static ssize_t vmm_fast_add_store(struct device *dev,
	struct device_attribute *attr, const char *buf,
	size_t n)
{
	return n;
}
DEVICE_ATTR_RW(vmm_fast_add);

ssize_t vmm_fast_multiply_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	s32 a, b, c, ret;

	get_random_bytes(&a, sizeof(s32));
	a &= 0xFF;
	get_random_bytes(&b, sizeof(s32));
	b &= 0xFF;
	get_random_bytes(&c, sizeof(s32));
	c &= 0xFF;
	ret = trusty_fast_call32(dev, MTEE_SMCNR(SMCF_FC_TEST_MULTIPLY, dev),
				 a, b, c);
	return scnprintf(buf, PAGE_SIZE, "%d * %d * %d = %d, %s\n", a, b, c,
			 ret, (a * b * c) == ret ? "PASS" : "FAIL");
}

static ssize_t vmm_fast_multiply_store(struct device *dev,
	struct device_attribute *attr, const char *buf,
	size_t n)
{
	return n;
}
DEVICE_ATTR_RW(vmm_fast_multiply);

static ssize_t vmm_std_add_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	s32 a, b, c, ret;

	get_random_bytes(&a, sizeof(s32));
	a &= 0xFF;
	get_random_bytes(&b, sizeof(s32));
	b &= 0xFF;
	get_random_bytes(&c, sizeof(s32));
	c &= 0xFF;
	ret = trusty_std_call32(dev, MTEE_SMCNR(SMCF_SC_TEST_ADD, dev),
						a, b, c);
	return scnprintf(buf, PAGE_SIZE, "%d + %d + %d = %d, %s\n", a, b, c,
			 ret, (a + b + c) == ret ? "PASS" : "FAIL");
}

static ssize_t vmm_std_add_store(struct device *dev,
	struct device_attribute *attr, const char *buf,
	size_t n)
{
	return n;
}
DEVICE_ATTR_RW(vmm_std_add);

static ssize_t vmm_std_multiply_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	s32 a, b, c, ret;

	get_random_bytes(&a, sizeof(s32));
	a &= 0xFF;
	get_random_bytes(&b, sizeof(s32));
	b &= 0xFF;
	get_random_bytes(&c, sizeof(s32));
	c &= 0xFF;
	ret = trusty_std_call32(dev, MTEE_SMCNR(SMCF_SC_TEST_MULTIPLY, dev),
				a, b, c);
	return scnprintf(buf, PAGE_SIZE, "%d * %d * %d = %d, %s\n", a, b, c,
			 ret, (a * b * c) == ret ? "PASS" : "FAIL");
}

static ssize_t vmm_std_multiply_store(struct device *dev,
	struct device_attribute *attr, const char *buf,
	size_t n)
{
	return n;
}
DEVICE_ATTR_RW(vmm_std_multiply);

static void nebula_create_debugfs(struct trusty_state *s, struct device *dev)
{
	int ret;

	pr_info("%s-%s\n", __func__, get_tee_name(s->tee_id));

	mtee_dev[s->tee_id] = dev;

	ret = device_create_file(dev, &dev_attr_vmm_fast_add);
	if (ret)
		goto err_create_vmm_fast_add;

	ret = device_create_file(dev, &dev_attr_vmm_fast_multiply);
	if (ret)
		goto err_create_vmm_fast_multiply;

	ret = device_create_file(dev, &dev_attr_vmm_std_add);
	if (ret)
		goto err_create_vmm_std_add;

	ret = device_create_file(dev, &dev_attr_vmm_std_multiply);
	if (ret)
		goto err_create_vmm_std_multiply;

	return;

err_create_vmm_std_multiply:
	device_remove_file(dev, &dev_attr_vmm_std_multiply);
err_create_vmm_std_add:
	device_remove_file(dev, &dev_attr_vmm_std_add);
err_create_vmm_fast_multiply:
	device_remove_file(dev, &dev_attr_vmm_fast_multiply);
err_create_vmm_fast_add:
	device_remove_file(dev, &dev_attr_vmm_fast_add);
}

void mtee_create_debugfs(struct trusty_state *s, struct device *dev)
{
	if (!s || !dev) {
		pr_info("[%s] Invalid input\n", __func__);
		return;
	}

	if (is_trusty_tee(s->tee_id))
		trusty_create_debugfs(s, dev);
	else if (is_nebula_tee(s->tee_id))
		nebula_create_debugfs(s, dev);
}
