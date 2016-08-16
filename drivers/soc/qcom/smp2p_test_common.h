/* drivers/soc/qcom/smp2p_test_common.h
 *
 * Copyright (c) 2013-2014,2016 The Linux Foundation. All rights reserved.
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
#ifndef _SMP2P_TEST_COMMON_H_
#define _SMP2P_TEST_COMMON_H_

#include <linux/debugfs.h>

/**
 * Unit test assertion for logging test cases.
 *
 * @a lval
 * @b rval
 * @cmp comparison operator
 *
 * Assertion fails if (@a cmp @b) is not true which then
 * logs the function and line number where the error occurred
 * along with the values of @a and @b.
 *
 * Assumes that the following local variables exist:
 * @s - sequential output file pointer
 * @failed - set to true if test fails
 */
#define UT_ASSERT_INT(a, cmp, b) \
	{ \
	int a_tmp = (a); \
	int b_tmp = (b); \
	if (!((a_tmp)cmp(b_tmp))) { \
		seq_printf(s, "%s:%d Fail: " #a "(%d) " #cmp " " #b "(%d)\n", \
				__func__, __LINE__, \
				a_tmp, b_tmp); \
		failed = 1; \
		break; \
	} \
	}

#define UT_ASSERT_PTR(a, cmp, b) \
	{ \
	void *a_tmp = (a); \
	void *b_tmp = (b); \
	if (!((a_tmp)cmp(b_tmp))) { \
		seq_printf(s, "%s:%d Fail: " #a "(%pK) " #cmp \
				" " #b "(%pK)\n", \
				__func__, __LINE__, \
				a_tmp, b_tmp); \
		failed = 1; \
		break; \
	} \
	}

#define UT_ASSERT_UINT(a, cmp, b) \
	{ \
	unsigned a_tmp = (a); \
	unsigned b_tmp = (b); \
	if (!((a_tmp)cmp(b_tmp))) { \
		seq_printf(s, "%s:%d Fail: " #a "(%u) " #cmp " " #b "(%u)\n", \
				__func__, __LINE__, \
				a_tmp, b_tmp); \
		failed = 1; \
		break; \
	} \
	}

#define UT_ASSERT_HEX(a, cmp, b) \
	{ \
	unsigned a_tmp = (a); \
	unsigned b_tmp = (b); \
	if (!((a_tmp)cmp(b_tmp))) { \
		seq_printf(s, "%s:%d Fail: " #a "(%x) " #cmp " " #b "(%x)\n", \
				__func__, __LINE__, \
				a_tmp, b_tmp); \
		failed = 1; \
		break; \
	} \
	}

/**
 * In-range unit test assertion for test cases.
 *
 * @a lval
 * @minv Minimum value
 * @maxv Maximum value
 *
 * Assertion fails if @a is not on the exclusive range minv, maxv
 * ((@a < @minv) or (@a > @maxv)).  In the failure case, the macro
 * logs the function and line number where the error occurred along
 * with the values of @a and @minv, @maxv.
 *
 * Assumes that the following local variables exist:
 * @s - sequential output file pointer
 * @failed - set to true if test fails
 */
#define UT_ASSERT_INT_IN_RANGE(a, minv, maxv) \
	{ \
	int a_tmp = (a); \
	int minv_tmp = (minv); \
	int maxv_tmp = (maxv); \
	if (((a_tmp) < (minv_tmp)) || ((a_tmp) > (maxv_tmp))) { \
		seq_printf(s, "%s:%d Fail: " #a "(%d) < " #minv "(%d) or " \
				 #a "(%d) > " #maxv "(%d)\n", \
				__func__, __LINE__, \
				a_tmp, minv_tmp, a_tmp, maxv_tmp); \
		failed = 1; \
		break; \
	} \
	}

/* Structure to track state changes for the notifier callback. */
struct mock_cb_data {
	bool initialized;
	spinlock_t lock;
	struct notifier_block nb;

	/* events */
	struct completion cb_completion;
	int cb_count;
	int event_open;
	int event_entry_update;
	struct msm_smp2p_update_notif entry_data;
};

void smp2p_debug_create(const char *name, void (*show)(struct seq_file *));
void smp2p_debug_create_u32(const char *name, uint32_t *value);
static inline int smp2p_test_notify(struct notifier_block *self,
	unsigned long event, void *data);

/**
 * Reset mock callback data to default values.
 *
 * @cb:  Mock callback data
 */
static inline void mock_cb_data_reset(struct mock_cb_data *cb)
{
	INIT_COMPLETION(cb->cb_completion);
	cb->cb_count = 0;
	cb->event_open = 0;
	cb->event_entry_update = 0;
	memset(&cb->entry_data, 0,
		sizeof(struct msm_smp2p_update_notif));
}


/**
 * Initialize mock callback data.
 *
 * @cb:  Mock callback data
 */
static inline void mock_cb_data_init(struct mock_cb_data *cb)
{
	if (!cb->initialized) {
		init_completion(&cb->cb_completion);
		spin_lock_init(&cb->lock);
		cb->initialized = true;
		cb->nb.notifier_call = smp2p_test_notify;
		memset(&cb->entry_data, 0,
			sizeof(struct msm_smp2p_update_notif));
	}
	mock_cb_data_reset(cb);
}

/**
 * Notifier function passed into SMP2P for testing.
 *
 * @self:       Pointer to calling notifier block
 * @event:	    Event
 * @data:       Event-specific data
 * @returns:    0
 */
static inline int smp2p_test_notify(struct notifier_block *self,
		unsigned long event, void *data)
{
	struct mock_cb_data *cb_data_ptr;
	unsigned long flags;

	cb_data_ptr = container_of(self, struct mock_cb_data, nb);

	spin_lock_irqsave(&cb_data_ptr->lock, flags);

	switch (event) {
	case SMP2P_OPEN:
		++cb_data_ptr->event_open;
		if (data) {
			cb_data_ptr->entry_data =
			*(struct msm_smp2p_update_notif *)(data);
		}
		break;
	case SMP2P_ENTRY_UPDATE:
		++cb_data_ptr->event_entry_update;
		if (data) {
			cb_data_ptr->entry_data =
			*(struct msm_smp2p_update_notif *)(data);
		}
		break;
	default:
		pr_err("%s Unknown event\n", __func__);
		break;
	}

	++cb_data_ptr->cb_count;
	complete(&cb_data_ptr->cb_completion);
	spin_unlock_irqrestore(&cb_data_ptr->lock, flags);
	return 0;
}
#endif /* _SMP2P_TEST_COMMON_H_ */
