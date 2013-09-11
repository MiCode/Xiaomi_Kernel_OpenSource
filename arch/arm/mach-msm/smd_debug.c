/* arch/arm/mach-msm/smd_debug.c
 *
 * Copyright (C) 2007 Google, Inc.
 * Copyright (c) 2009-2013, The Linux Foundation. All rights reserved.
 * Author: Brian Swetland <swetland@google.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/debugfs.h>
#include <linux/list.h>
#include <linux/ctype.h>
#include <linux/jiffies.h>

#include <mach/msm_iomap.h>
#include <mach/msm_smem.h>

#include "smd_private.h"
#include "smem_private.h"

#if defined(CONFIG_DEBUG_FS)

static char *chstate(unsigned n)
{
	switch (n) {
	case SMD_SS_CLOSED:
		return "CLOSED";
	case SMD_SS_OPENING:
		return "OPENING";
	case SMD_SS_OPENED:
		return "OPENED";
	case SMD_SS_FLUSHING:
		return "FLUSHING";
	case SMD_SS_CLOSING:
		return "CLOSING";
	case SMD_SS_RESET:
		return "RESET";
	case SMD_SS_RESET_OPENING:
		return "ROPENING";
	default:
		return "UNKNOWN";
	}
}

static void debug_f3(struct seq_file *s)
{
	char *x;
	int size;
	int j = 0;
	unsigned cols = 0;
	char str[4*sizeof(unsigned)+1] = {0};

	seq_puts(s, "Printing to log\n");

	x = smem_get_entry(SMEM_ERR_F3_TRACE_LOG, &size);
	if (x != 0) {
		pr_info("smem: F3 TRACE LOG\n");
		while (size > 0) {
			if (size >= sizeof(unsigned)) {
				pr_info("%08x", *((unsigned *) x));
				for (j = 0; j < sizeof(unsigned); ++j)
					if (isprint(*(x+j)))
						str[cols*sizeof(unsigned) + j]
							= *(x+j);
					else
						str[cols*sizeof(unsigned) + j]
							= '-';
				x += sizeof(unsigned);
				size -= sizeof(unsigned);
			} else {
				while (size-- > 0)
					pr_info("%02x", (unsigned) *x++);
				break;
			}
			if (cols == 3) {
				cols = 0;
				str[4*sizeof(unsigned)] = 0;
				pr_info(" %s\n", str);
				str[0] = 0;
			} else {
				cols++;
				pr_info(" ");
			}
		}
		pr_info("\n");
	}
}

static void debug_int_stats(struct seq_file *s)
{
	int subsys;
	struct interrupt_stat *stats = interrupt_stats;
	const char *subsys_name;

	seq_puts(s,
		"   Subsystem    | Interrupt ID |    In     | Out (Hardcoded) |"
		" Out (Configured)|\n");

	for (subsys = 0; subsys < NUM_SMD_SUBSYSTEMS; ++subsys) {
		subsys_name = smd_pid_to_subsystem(subsys);
		if (subsys_name) {
			seq_printf(s,
				"%-10s %4s |    %9d | %9u |       %9u |       %9u |\n",
				smd_pid_to_subsystem(subsys), "smd",
				stats->smd_interrupt_id,
				stats->smd_in_count,
				stats->smd_out_hardcode_count,
				stats->smd_out_config_count);

			seq_printf(s,
				"%-10s %4s |    %9d | %9u |       %9u |       %9u |\n",
				smd_pid_to_subsystem(subsys), "smsm",
				stats->smsm_interrupt_id,
				stats->smsm_in_count,
				stats->smsm_out_hardcode_count,
				stats->smsm_out_config_count);
		}
		++stats;
	}
}

static void debug_int_stats_reset(struct seq_file *s)
{
	int subsys;
	struct interrupt_stat *stats = interrupt_stats;

	seq_puts(s, "Resetting interrupt stats.\n");

	for (subsys = 0; subsys < NUM_SMD_SUBSYSTEMS; ++subsys) {
		stats->smd_in_count = 0;
		stats->smd_out_hardcode_count = 0;
		stats->smd_out_config_count = 0;
		stats->smsm_in_count = 0;
		stats->smsm_out_hardcode_count = 0;
		stats->smsm_out_config_count = 0;
		++stats;
	}
}

static void debug_diag(struct seq_file *s)
{
	seq_puts(s, "Printing to log\n");
	smd_diag();
}

static void debug_modem_err_f3(struct seq_file *s)
{
	char *x;
	int size;
	int j = 0;
	unsigned cols = 0;
	char str[4*sizeof(unsigned)+1] = {0};

	x = smem_get_entry(SMEM_ERR_F3_TRACE_LOG, &size);
	if (x != 0) {
		pr_info("smem: F3 TRACE LOG\n");
		while (size > 0) {
			if (size >= sizeof(unsigned)) {
				seq_printf(s, "%08x",
					       *((unsigned *) x));
				for (j = 0; j < sizeof(unsigned); ++j)
					if (isprint(*(x+j)))
						str[cols*sizeof(unsigned) + j]
							= *(x+j);
					else
						str[cols*sizeof(unsigned) + j]
							= '-';
				x += sizeof(unsigned);
				size -= sizeof(unsigned);
			} else {
				while (size-- > 0)
					seq_printf(s, "%02x", (unsigned) *x++);
				break;
			}
			if (cols == 3) {
				cols = 0;
				str[4*sizeof(unsigned)] = 0;
				seq_printf(s, " %s\n", str);
				str[0] = 0;
			} else {
				cols++;
				seq_puts(s, " ");
			}
		}
		seq_puts(s, "\n");
	}
}

static void debug_modem_err(struct seq_file *s)
{
	char *x;
	int size;

	x = smem_find(ID_DIAG_ERR_MSG, SZ_DIAG_ERR_MSG);
	if (x != 0) {
		x[SZ_DIAG_ERR_MSG - 1] = 0;
		seq_printf(s, "smem: DIAG '%s'\n", x);
	}

	x = smem_get_entry(SMEM_ERR_CRASH_LOG, &size);
	if (x != 0) {
		x[size - 1] = 0;
		seq_printf(s, "smem: CRASH LOG\n'%s'\n", x);
	}
	seq_puts(s, "\n");
}

static void debug_read_diag_msg(struct seq_file *s)
{
	char *msg;

	msg = smem_find(ID_DIAG_ERR_MSG, SZ_DIAG_ERR_MSG);

	if (msg) {
		msg[SZ_DIAG_ERR_MSG - 1] = 0;
		seq_printf(s, "diag: '%s'\n", msg);
	}
}

static void dump_ch(struct seq_file *s, int n,
		   void *half_ch_s,
		   void *half_ch_r,
		   struct smd_half_channel_access *half_ch_funcs,
		   unsigned size)
{
	seq_printf(s, "ch%02d:"
		" %8s(%04d/%04d) %c%c%c%c%c%c%c%c <->"
		" %8s(%04d/%04d) %c%c%c%c%c%c%c%c : %5x\n", n,
		chstate(half_ch_funcs->get_state(half_ch_s)),
		half_ch_funcs->get_tail(half_ch_s),
		half_ch_funcs->get_head(half_ch_s),
		half_ch_funcs->get_fDSR(half_ch_s) ? 'D' : 'd',
		half_ch_funcs->get_fCTS(half_ch_s) ? 'C' : 'c',
		half_ch_funcs->get_fCD(half_ch_s) ? 'C' : 'c',
		half_ch_funcs->get_fRI(half_ch_s) ? 'I' : 'i',
		half_ch_funcs->get_fHEAD(half_ch_s) ? 'W' : 'w',
		half_ch_funcs->get_fTAIL(half_ch_s) ? 'R' : 'r',
		half_ch_funcs->get_fSTATE(half_ch_s) ? 'S' : 's',
		half_ch_funcs->get_fBLOCKREADINTR(half_ch_s) ? 'B' : 'b',
		chstate(half_ch_funcs->get_state(half_ch_r)),
		half_ch_funcs->get_tail(half_ch_r),
		half_ch_funcs->get_head(half_ch_r),
		half_ch_funcs->get_fDSR(half_ch_r) ? 'D' : 'd',
		half_ch_funcs->get_fCTS(half_ch_r) ? 'C' : 'c',
		half_ch_funcs->get_fCD(half_ch_r) ? 'C' : 'c',
		half_ch_funcs->get_fRI(half_ch_r) ? 'I' : 'i',
		half_ch_funcs->get_fHEAD(half_ch_r) ? 'W' : 'w',
		half_ch_funcs->get_fTAIL(half_ch_r) ? 'R' : 'r',
		half_ch_funcs->get_fSTATE(half_ch_r) ? 'S' : 's',
		half_ch_funcs->get_fBLOCKREADINTR(half_ch_r) ? 'B' : 'b',
		size
		);
}

static void debug_read_smsm_state(struct seq_file *s)
{
	uint32_t *smsm;
	int n;

	smsm = smem_find(ID_SHARED_STATE,
			 SMSM_NUM_ENTRIES * sizeof(uint32_t));

	if (smsm)
		for (n = 0; n < SMSM_NUM_ENTRIES; n++)
			seq_printf(s, "entry %d: 0x%08x\n", n, smsm[n]);
}

struct SMSM_CB_DATA {
	int cb_count;
	void *data;
	uint32_t old_state;
	uint32_t new_state;
};
static struct SMSM_CB_DATA smsm_cb_data;
static struct completion smsm_cb_completion;

static void smsm_state_cb(void *data, uint32_t old_state, uint32_t new_state)
{
	smsm_cb_data.cb_count++;
	smsm_cb_data.old_state = old_state;
	smsm_cb_data.new_state = new_state;
	smsm_cb_data.data = data;
	complete_all(&smsm_cb_completion);
}

#define UT_EQ_INT(a, b) \
	if ((a) != (b)) { \
		seq_printf(s, "%s:%d " #a "(%d) != " #b "(%d)\n", \
				__func__, __LINE__, \
				a, b); \
		break; \
	} \
	do {} while (0)

#define UT_GT_INT(a, b) \
	if ((a) <= (b)) { \
		seq_printf(s, "%s:%d " #a "(%d) > " #b "(%d)\n", \
				__func__, __LINE__, \
				a, b); \
		break; \
	} \
	do {} while (0)

#define SMSM_CB_TEST_INIT() \
	do { \
		smsm_cb_data.cb_count = 0; \
		smsm_cb_data.old_state = 0; \
		smsm_cb_data.new_state = 0; \
		smsm_cb_data.data = 0; \
	} while (0)


static void debug_test_smsm(struct seq_file *s)
{
	int test_num = 0;
	int ret;

	/* Test case 1 - Register new callback for notification */
	do {
		test_num++;
		SMSM_CB_TEST_INIT();
		ret = smsm_state_cb_register(SMSM_APPS_STATE, SMSM_SMDINIT,
				smsm_state_cb, (void *)0x1234);
		UT_EQ_INT(ret, 0);

		/* de-assert SMSM_SMD_INIT to trigger state update */
		UT_EQ_INT(smsm_cb_data.cb_count, 0);
		INIT_COMPLETION(smsm_cb_completion);
		smsm_change_state(SMSM_APPS_STATE, SMSM_SMDINIT, 0x0);
		UT_GT_INT((int)wait_for_completion_timeout(&smsm_cb_completion,
					msecs_to_jiffies(20)), 0);

		UT_EQ_INT(smsm_cb_data.cb_count, 1);
		UT_EQ_INT(smsm_cb_data.old_state & SMSM_SMDINIT, SMSM_SMDINIT);
		UT_EQ_INT(smsm_cb_data.new_state & SMSM_SMDINIT, 0x0);
		UT_EQ_INT((int)smsm_cb_data.data, 0x1234);

		/* re-assert SMSM_SMD_INIT to trigger state update */
		INIT_COMPLETION(smsm_cb_completion);
		smsm_change_state(SMSM_APPS_STATE, 0x0, SMSM_SMDINIT);
		UT_GT_INT((int)wait_for_completion_timeout(&smsm_cb_completion,
					msecs_to_jiffies(20)), 0);
		UT_EQ_INT(smsm_cb_data.cb_count, 2);
		UT_EQ_INT(smsm_cb_data.old_state & SMSM_SMDINIT, 0x0);
		UT_EQ_INT(smsm_cb_data.new_state & SMSM_SMDINIT, SMSM_SMDINIT);

		/* deregister callback */
		ret = smsm_state_cb_deregister(SMSM_APPS_STATE, SMSM_SMDINIT,
				smsm_state_cb, (void *)0x1234);
		UT_EQ_INT(ret, 2);

		/* make sure state change doesn't cause any more callbacks */
		INIT_COMPLETION(smsm_cb_completion);
		smsm_change_state(SMSM_APPS_STATE, SMSM_SMDINIT, 0x0);
		smsm_change_state(SMSM_APPS_STATE, 0x0, SMSM_SMDINIT);
		UT_EQ_INT((int)wait_for_completion_timeout(&smsm_cb_completion,
					msecs_to_jiffies(20)), 0);
		UT_EQ_INT(smsm_cb_data.cb_count, 2);

		seq_printf(s, "Test %d - PASS\n", test_num);
	} while (0);

	/* Test case 2 - Update already registered callback */
	do {
		test_num++;
		SMSM_CB_TEST_INIT();
		ret = smsm_state_cb_register(SMSM_APPS_STATE, SMSM_SMDINIT,
				smsm_state_cb, (void *)0x1234);
		UT_EQ_INT(ret, 0);
		ret = smsm_state_cb_register(SMSM_APPS_STATE, SMSM_INIT,
				smsm_state_cb, (void *)0x1234);
		UT_EQ_INT(ret, 1);

		/* verify both callback bits work */
		INIT_COMPLETION(smsm_cb_completion);
		UT_EQ_INT(smsm_cb_data.cb_count, 0);
		smsm_change_state(SMSM_APPS_STATE, SMSM_SMDINIT, 0x0);
		UT_GT_INT((int)wait_for_completion_timeout(&smsm_cb_completion,
					msecs_to_jiffies(20)), 0);
		UT_EQ_INT(smsm_cb_data.cb_count, 1);
		INIT_COMPLETION(smsm_cb_completion);
		smsm_change_state(SMSM_APPS_STATE, 0x0, SMSM_SMDINIT);
		UT_GT_INT((int)wait_for_completion_timeout(&smsm_cb_completion,
					msecs_to_jiffies(20)), 0);
		UT_EQ_INT(smsm_cb_data.cb_count, 2);

		INIT_COMPLETION(smsm_cb_completion);
		smsm_change_state(SMSM_APPS_STATE, SMSM_INIT, 0x0);
		UT_GT_INT((int)wait_for_completion_timeout(&smsm_cb_completion,
					msecs_to_jiffies(20)), 0);
		UT_EQ_INT(smsm_cb_data.cb_count, 3);
		INIT_COMPLETION(smsm_cb_completion);
		smsm_change_state(SMSM_APPS_STATE, 0x0, SMSM_INIT);
		UT_GT_INT((int)wait_for_completion_timeout(&smsm_cb_completion,
					msecs_to_jiffies(20)), 0);
		UT_EQ_INT(smsm_cb_data.cb_count, 4);

		/* deregister 1st callback */
		ret = smsm_state_cb_deregister(SMSM_APPS_STATE, SMSM_SMDINIT,
				smsm_state_cb, (void *)0x1234);
		UT_EQ_INT(ret, 1);
		INIT_COMPLETION(smsm_cb_completion);
		smsm_change_state(SMSM_APPS_STATE, SMSM_SMDINIT, 0x0);
		smsm_change_state(SMSM_APPS_STATE, 0x0, SMSM_SMDINIT);
		UT_EQ_INT((int)wait_for_completion_timeout(&smsm_cb_completion,
					msecs_to_jiffies(20)), 0);
		UT_EQ_INT(smsm_cb_data.cb_count, 4);

		INIT_COMPLETION(smsm_cb_completion);
		smsm_change_state(SMSM_APPS_STATE, SMSM_INIT, 0x0);
		UT_GT_INT((int)wait_for_completion_timeout(&smsm_cb_completion,
					msecs_to_jiffies(20)), 0);
		UT_EQ_INT(smsm_cb_data.cb_count, 5);
		INIT_COMPLETION(smsm_cb_completion);
		smsm_change_state(SMSM_APPS_STATE, 0x0, SMSM_INIT);
		UT_GT_INT((int)wait_for_completion_timeout(&smsm_cb_completion,
					msecs_to_jiffies(20)), 0);
		UT_EQ_INT(smsm_cb_data.cb_count, 6);

		/* deregister 2nd callback */
		ret = smsm_state_cb_deregister(SMSM_APPS_STATE, SMSM_INIT,
				smsm_state_cb, (void *)0x1234);
		UT_EQ_INT(ret, 2);

		/* make sure state change doesn't cause any more callbacks */
		INIT_COMPLETION(smsm_cb_completion);
		smsm_change_state(SMSM_APPS_STATE, SMSM_INIT, 0x0);
		smsm_change_state(SMSM_APPS_STATE, 0x0, SMSM_INIT);
		UT_EQ_INT((int)wait_for_completion_timeout(&smsm_cb_completion,
					msecs_to_jiffies(20)), 0);
		UT_EQ_INT(smsm_cb_data.cb_count, 6);

		seq_printf(s, "Test %d - PASS\n", test_num);
	} while (0);

	/* Test case 3 - Two callback registrations with different data */
	do {
		test_num++;
		SMSM_CB_TEST_INIT();
		ret = smsm_state_cb_register(SMSM_APPS_STATE, SMSM_SMDINIT,
				smsm_state_cb, (void *)0x1234);
		UT_EQ_INT(ret, 0);
		ret = smsm_state_cb_register(SMSM_APPS_STATE, SMSM_INIT,
				smsm_state_cb, (void *)0x3456);
		UT_EQ_INT(ret, 0);

		/* verify both callbacks work */
		INIT_COMPLETION(smsm_cb_completion);
		UT_EQ_INT(smsm_cb_data.cb_count, 0);
		smsm_change_state(SMSM_APPS_STATE, SMSM_SMDINIT, 0x0);
		UT_GT_INT((int)wait_for_completion_timeout(&smsm_cb_completion,
					msecs_to_jiffies(20)), 0);
		UT_EQ_INT(smsm_cb_data.cb_count, 1);
		UT_EQ_INT((int)smsm_cb_data.data, 0x1234);

		INIT_COMPLETION(smsm_cb_completion);
		smsm_change_state(SMSM_APPS_STATE, SMSM_INIT, 0x0);
		UT_GT_INT((int)wait_for_completion_timeout(&smsm_cb_completion,
					msecs_to_jiffies(20)), 0);
		UT_EQ_INT(smsm_cb_data.cb_count, 2);
		UT_EQ_INT((int)smsm_cb_data.data, 0x3456);

		/* cleanup and unregister
		 * degregister in reverse to verify data field is
		 * being used
		 */
		smsm_change_state(SMSM_APPS_STATE, 0x0, SMSM_SMDINIT);
		smsm_change_state(SMSM_APPS_STATE, 0x0, SMSM_INIT);
		ret = smsm_state_cb_deregister(SMSM_APPS_STATE,
				SMSM_INIT,
				smsm_state_cb, (void *)0x3456);
		UT_EQ_INT(ret, 2);
		ret = smsm_state_cb_deregister(SMSM_APPS_STATE,
				SMSM_SMDINIT,
				smsm_state_cb, (void *)0x1234);
		UT_EQ_INT(ret, 2);

		seq_printf(s, "Test %d - PASS\n", test_num);
	} while (0);
}

static void debug_read_mem(struct seq_file *s)
{
	unsigned n;
	struct smem_shared *shared = (void *) MSM_SHARED_RAM_BASE;
	struct smem_heap_entry *toc = shared->heap_toc;

	seq_printf(s, "heap: init=%d free=%d remain=%d\n",
		       shared->heap_info.initialized,
		       shared->heap_info.free_offset,
		       shared->heap_info.heap_remaining);

	for (n = 0; n < SMEM_NUM_ITEMS; n++) {
		if (toc[n].allocated == 0)
			continue;
		seq_printf(s, "%04d: offset %08x size %08x\n",
			       n, toc[n].offset, toc[n].size);
	}
}

#if (!defined(CONFIG_MSM_SMD_PKG4) && !defined(CONFIG_MSM_SMD_PKG3))
static void debug_read_ch(struct seq_file *s)
{
	void *shared;
	int n;
	struct smd_alloc_elm *ch_tbl;
	unsigned ch_type;
	unsigned shared_size;

	ch_tbl = smem_find(ID_CH_ALLOC_TBL, sizeof(*ch_tbl) * 64);
	if (!ch_tbl)
		return;

	for (n = 0; n < SMD_CHANNELS; n++) {
		ch_type = SMD_CHANNEL_TYPE(ch_tbl[n].type);
		if (is_word_access_ch(ch_type))
			shared_size =
				sizeof(struct smd_half_channel_word_access);
		else
			shared_size = sizeof(struct smd_half_channel);
		shared = smem_find(ID_SMD_CHANNELS + n,
				2 * shared_size + SMD_BUF_SIZE);

		if (shared == 0)
			continue;
		dump_ch(s, n, shared,
			     (shared + shared_size +
			     SMD_BUF_SIZE), get_half_ch_funcs(ch_type),
			     SMD_BUF_SIZE);
	}
}
#else
static void debug_read_ch(struct seq_file *s)
{
	void *shared, *buffer;
	unsigned buffer_sz;
	int n;
	struct smd_alloc_elm *ch_tbl;
	unsigned ch_type;
	unsigned shared_size;

	ch_tbl = smem_find(ID_CH_ALLOC_TBL, sizeof(*ch_tbl) * 64);
	if (!ch_tbl)
		return;

	for (n = 0; n < SMD_CHANNELS; n++) {
		ch_type = SMD_CHANNEL_TYPE(ch_tbl[n].type);
		if (is_word_access_ch(ch_type))
			shared_size =
				sizeof(struct smd_half_channel_word_access);
		else
			shared_size = sizeof(struct smd_half_channel);

		shared = smem_find(ID_SMD_CHANNELS + n, 2 * shared_size);

		if (shared == 0)
			continue;

		buffer = smem_get_entry(SMEM_SMD_FIFO_BASE_ID + n, &buffer_sz);

		if (buffer == 0)
			continue;

		dump_ch(s, n, shared,
			     (shared + shared_size),
			     get_half_ch_funcs(ch_type),
			     buffer_sz / 2);
	}
}
#endif

static void debug_read_smem_version(struct seq_file *s)
{
	struct smem_shared *shared = (void *) MSM_SHARED_RAM_BASE;
	uint32_t n, version;

	for (n = 0; n < 32; n++) {
		version = shared->version[n];
		seq_printf(s, "entry %d: smem = %d  proc_comm = %d\n", n,
			       version >> 16,
			       version & 0xffff);
	}
}

/* NNV: revist, it may not be smd version */
static void debug_read_smd_version(struct seq_file *s)
{
	uint32_t *smd_ver;
	uint32_t n, version;

	smd_ver = smem_alloc(SMEM_VERSION_SMD, 32 * sizeof(uint32_t));

	if (smd_ver)
		for (n = 0; n < 32; n++) {
			version = smd_ver[n];
			seq_printf(s, "entry %d: %d.%d\n", n,
				       version >> 16,
				       version & 0xffff);
		}
}

static void debug_read_build_id(struct seq_file *s)
{
	unsigned size;
	void *data;

	data = smem_get_entry(SMEM_HW_SW_BUILD_ID, &size);
	if (!data)
		return;

	seq_write(s, data, size);
}

static void debug_read_alloc_tbl(struct seq_file *s)
{
	struct smd_alloc_elm *shared;
	int n;

	shared = smem_find(ID_CH_ALLOC_TBL, sizeof(struct smd_alloc_elm[64]));

	if (!shared)
		return;

	for (n = 0; n < 64; n++) {
		seq_printf(s, "name=%s cid=%d ch type=%d "
				"xfer type=%d ref_count=%d\n",
				shared[n].name,
				shared[n].cid,
				SMD_CHANNEL_TYPE(shared[n].type),
				SMD_XFER_TYPE(shared[n].type),
				shared[n].ref_count);
	}
}

static void debug_read_intr_mask(struct seq_file *s)
{
	uint32_t *smsm;
	int m, n;

	smsm = smem_alloc(SMEM_SMSM_CPU_INTR_MASK,
			  SMSM_NUM_ENTRIES * SMSM_NUM_HOSTS * sizeof(uint32_t));

	if (smsm)
		for (m = 0; m < SMSM_NUM_ENTRIES; m++) {
			seq_printf(s, "entry %d:", m);
			for (n = 0; n < SMSM_NUM_HOSTS; n++)
				seq_printf(s, "   host %d: 0x%08x",
					       n, smsm[m * SMSM_NUM_HOSTS + n]);
			seq_puts(s, "\n");
		}
}

static void debug_read_intr_mux(struct seq_file *s)
{
	uint32_t *smsm;
	int n;

	smsm = smem_alloc(SMEM_SMD_SMSM_INTR_MUX,
			  SMSM_NUM_INTR_MUX * sizeof(uint32_t));

	if (smsm)
		for (n = 0; n < SMSM_NUM_INTR_MUX; n++)
			seq_printf(s, "entry %d: %d\n", n, smsm[n]);
}

static int debugfs_show(struct seq_file *s, void *data)
{
	void (*show)(struct seq_file *) = s->private;

	show(s);

	return 0;
}

static int debug_open(struct inode *inode, struct file *file)
{
	return single_open(file, debugfs_show, inode->i_private);
}

static const struct file_operations debug_ops = {
	.open = debug_open,
	.release = single_release,
	.read = seq_read,
	.llseek = seq_lseek,
};

static void debug_create(const char *name, umode_t mode,
			 struct dentry *dent,
			 void (*show)(struct seq_file *))
{
	struct dentry *file;

	file = debugfs_create_file(name, mode, dent, show, &debug_ops);
	if (!file)
		pr_err("%s: unable to create file '%s'\n", __func__, name);
}

static int __init smd_debugfs_init(void)
{
	struct dentry *dent;

	dent = debugfs_create_dir("smd", 0);
	if (IS_ERR(dent))
		return PTR_ERR(dent);

	debug_create("ch", 0444, dent, debug_read_ch);
	debug_create("diag", 0444, dent, debug_read_diag_msg);
	debug_create("mem", 0444, dent, debug_read_mem);
	debug_create("version", 0444, dent, debug_read_smd_version);
	debug_create("tbl", 0444, dent, debug_read_alloc_tbl);
	debug_create("modem_err", 0444, dent, debug_modem_err);
	debug_create("modem_err_f3", 0444, dent, debug_modem_err_f3);
	debug_create("print_diag", 0444, dent, debug_diag);
	debug_create("print_f3", 0444, dent, debug_f3);
	debug_create("int_stats", 0444, dent, debug_int_stats);
	debug_create("int_stats_reset", 0444, dent, debug_int_stats_reset);

	/* NNV: this is google only stuff */
	debug_create("build", 0444, dent, debug_read_build_id);

	return 0;
}

static int __init smsm_debugfs_init(void)
{
	struct dentry *dent;

	dent = debugfs_create_dir("smsm", 0);
	if (IS_ERR(dent))
		return PTR_ERR(dent);

	debug_create("state", 0444, dent, debug_read_smsm_state);
	debug_create("intr_mask", 0444, dent, debug_read_intr_mask);
	debug_create("intr_mux", 0444, dent, debug_read_intr_mux);
	debug_create("version", 0444, dent, debug_read_smem_version);
	debug_create("smsm_test", 0444, dent, debug_test_smsm);

	init_completion(&smsm_cb_completion);

	return 0;
}

late_initcall(smd_debugfs_init);
late_initcall(smsm_debugfs_init);
#endif


#define MAX_NUM_SLEEP_CLIENTS		64
#define MAX_SLEEP_NAME_LEN		8

#define NUM_GPIO_INT_REGISTERS		6
#define GPIO_SMEM_NUM_GROUPS		2
#define GPIO_SMEM_MAX_PC_INTERRUPTS	8

struct tramp_gpio_save {
	unsigned int enable;
	unsigned int detect;
	unsigned int polarity;
};

struct tramp_gpio_smem {
	uint16_t num_fired[GPIO_SMEM_NUM_GROUPS];
	uint16_t fired[GPIO_SMEM_NUM_GROUPS][GPIO_SMEM_MAX_PC_INTERRUPTS];
	uint32_t enabled[NUM_GPIO_INT_REGISTERS];
	uint32_t detection[NUM_GPIO_INT_REGISTERS];
	uint32_t polarity[NUM_GPIO_INT_REGISTERS];
};

/*
 * Print debug information on shared memory sleep variables
 */
void smsm_print_sleep_info(uint32_t sleep_delay, uint32_t sleep_limit,
	uint32_t irq_mask, uint32_t wakeup_reason, uint32_t pending_irqs)
{
	unsigned long flags;
	uint32_t *ptr;
	struct tramp_gpio_smem *gpio;

	spin_lock_irqsave(&smem_lock, flags);

	pr_info("SMEM_SMSM_SLEEP_DELAY: %x\n", sleep_delay);
	pr_info("SMEM_SMSM_LIMIT_SLEEP: %x\n", sleep_limit);

	ptr = smem_alloc(SMEM_SLEEP_POWER_COLLAPSE_DISABLED, sizeof(*ptr));
	if (ptr)
		pr_info("SMEM_SLEEP_POWER_COLLAPSE_DISABLED: %x\n", *ptr);
	else
		pr_info("SMEM_SLEEP_POWER_COLLAPSE_DISABLED: missing\n");

	pr_info("SMEM_SMSM_INT_INFO %x %x %x\n",
		irq_mask, pending_irqs, wakeup_reason);

	gpio = smem_alloc(SMEM_GPIO_INT, sizeof(*gpio));
	if (gpio) {
		int i;
		for (i = 0; i < NUM_GPIO_INT_REGISTERS; i++)
			pr_info("SMEM_GPIO_INT: %d: e %x d %x p %x\n",
				i, gpio->enabled[i], gpio->detection[i],
				gpio->polarity[i]);

		for (i = 0; i < GPIO_SMEM_NUM_GROUPS; i++)
			pr_info("SMEM_GPIO_INT: %d: f %d: %d %d...\n",
				i, gpio->num_fired[i], gpio->fired[i][0],
				gpio->fired[i][1]);
	} else
		pr_info("SMEM_GPIO_INT: missing\n");

	spin_unlock_irqrestore(&smem_lock, flags);
}
