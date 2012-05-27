/* drivers/tty/smux_test.c
 *
 * Copyright (c) 2012, Code Aurora Forum. All rights reserved.
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
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/completion.h>
#include <linux/termios.h>
#include <linux/smux.h>
#include "smux_private.h"

#define DEBUG_BUFMAX 4096

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
 * @buf - buffer to write failure message to
 * @i - number of bytes written to buffer
 * @max - maximum size of the buffer
 * @failed - set to true if test fails
 */
#define UT_ASSERT_INT(a, cmp, b) \
	if (!((a)cmp(b))) { \
		i += scnprintf(buf + i, max - i, \
			"%s:%d Fail: " #a "(%d) " #cmp " " #b "(%d)\n", \
				__func__, __LINE__, \
				a, b); \
		failed = 1; \
		break; \
	} \
	do {} while (0)

#define UT_ASSERT_PTR(a, cmp, b) \
	if (!((a)cmp(b))) { \
		i += scnprintf(buf + i, max - i, \
			"%s:%d Fail: " #a "(%p) " #cmp " " #b "(%p)\n", \
				__func__, __LINE__, \
				a, b); \
		failed = 1; \
		break; \
	} \
	do {} while (0)

#define UT_ASSERT_UINT(a, cmp, b) \
	if (!((a)cmp(b))) { \
		i += scnprintf(buf + i, max - i, \
			"%s:%d Fail: " #a "(%u) " #cmp " " #b "(%u)\n", \
				__func__, __LINE__, \
				a, b); \
		failed = 1; \
		break; \
	} \
	do {} while (0)

static unsigned char test_array[] = {1, 1, 2, 3, 5, 8, 13, 21, 34, 55,
					89, 144, 233};

/* Used for mapping local to remote TIOCM signals */
struct tiocm_test_vector {
	uint32_t input;
	uint32_t set_old;
	uint32_t set_new;
	uint32_t clr_old;
};

/**
 * Allocates a new buffer for SMUX for every call.
 */
int get_rx_buffer(void *priv, void **pkt_priv, void **buffer, int size)
{
	void *rx_buf;

	rx_buf = kmalloc(size, GFP_ATOMIC);
	*pkt_priv = (void *)0x1234;
	*buffer = rx_buf;

	return 0;
}

/* Test vector for packet tests. */
struct test_vector {
	const char *data;
	const unsigned len;
};

/* Mock object metadata for SMUX_READ_DONE event */
struct mock_read_event {
	struct list_head list;
	struct smux_meta_read meta;
};

/* Mock object metadata for SMUX_WRITE_DONE event */
struct mock_write_event {
	struct list_head list;
	struct smux_meta_write meta;
};

/* Mock object for all SMUX callback events */
struct smux_mock_callback {
	int cb_count;
	struct completion cb_completion;
	spinlock_t lock;

	/* status changes */
	int event_connected;
	int event_disconnected;
	int event_disconnected_ssr;
	int event_low_wm;
	int event_high_wm;

	/* TIOCM changes */
	int event_tiocm;
	struct smux_meta_tiocm tiocm_meta;

	/* read event data */
	int event_read_done;
	int event_read_failed;
	struct list_head read_events;

	/* write event data */
	int event_write_done;
	int event_write_failed;
	struct list_head write_events;
};

/**
 * Initialize mock callback data. Only call once.
 *
 * @cb  Mock callback data
 */
void mock_cb_data_init(struct smux_mock_callback *cb)
{
	init_completion(&cb->cb_completion);
	spin_lock_init(&cb->lock);
	INIT_LIST_HEAD(&cb->read_events);
	INIT_LIST_HEAD(&cb->write_events);
}

/**
 * Reset mock callback data to default values.
 *
 * @cb  Mock callback data
 *
 * All packets are freed and counters reset to zero.
 */
void mock_cb_data_reset(struct smux_mock_callback *cb)
{
	cb->cb_count = 0;
	INIT_COMPLETION(cb->cb_completion);
	cb->event_connected = 0;
	cb->event_disconnected = 0;
	cb->event_disconnected_ssr = 0;
	cb->event_low_wm = 0;
	cb->event_high_wm = 0;
	cb->event_tiocm = 0;
	cb->tiocm_meta.tiocm_old = 0;
	cb->tiocm_meta.tiocm_new = 0;

	cb->event_read_done = 0;
	cb->event_read_failed = 0;
	while (!list_empty(&cb->read_events)) {
		struct mock_read_event *meta;
		meta = list_first_entry(&cb->read_events,
				struct mock_read_event,
				list);
		kfree(meta->meta.buffer);
		list_del(&meta->list);
		kfree(meta);
	}

	cb->event_write_done = 0;
	cb->event_write_failed = 0;
	while (!list_empty(&cb->write_events)) {
		struct mock_write_event *meta;
		meta = list_first_entry(&cb->write_events,
				struct mock_write_event,
				list);
		list_del(&meta->list);
		kfree(meta);
	}
}

/**
 * Dump the values of the mock callback data for debug purposes.
 *
 * @cb  Mock callback data
 * @buf Print buffer
 * @max Maximum number of characters to print
 *
 * @returns Number of characters added to buffer
 */
static int mock_cb_data_print(const struct smux_mock_callback *cb,
		char *buf, int max)
{
	int i = 0;

	i += scnprintf(buf + i, max - i,
		"\tcb_count=%d\n"
		"\tcb_completion.done=%d\n"
		"\tevent_connected=%d\n"
		"\tevent_disconnected=%d\n"
		"\tevent_disconnected_ssr=%d\n"
		"\tevent_low_wm=%d\n"
		"\tevent_high_wm=%d\n"
		"\tevent_tiocm=%d\n"
		"\tevent_read_done=%d\n"
		"\tevent_read_failed=%d\n"
		"\tread_events=%d\n"
		"\tevent_write_done=%d\n"
		"\tevent_write_failed=%d\n"
		"\twrite_events=%d\n",
		cb->cb_count,
		cb->cb_completion.done,
		cb->event_connected,
		cb->event_disconnected,
		cb->event_disconnected_ssr,
		cb->event_low_wm,
		cb->event_high_wm,
		cb->event_tiocm,
		cb->event_read_done,
		cb->event_read_failed,
		!list_empty(&cb->read_events),
		cb->event_write_done,
		cb->event_write_failed,
		list_empty(&cb->write_events)
		);

	return i;
}

/**
 * Mock object event callback.  Used to logs events for analysis in the unit
 * tests.
 */
void smux_mock_cb(void *priv, int event, const void *metadata)
{
	struct smux_mock_callback *cb_data_ptr;
	struct mock_write_event *write_event_meta;
	struct mock_read_event *read_event_meta;
	unsigned long flags;

	cb_data_ptr = (struct smux_mock_callback *)priv;
	if (cb_data_ptr == NULL) {
		pr_err("%s: invalid private data\n", __func__);
		return;
	}

	spin_lock_irqsave(&cb_data_ptr->lock, flags);
	switch (event) {
	case SMUX_CONNECTED:
		++cb_data_ptr->event_connected;
		break;

	case SMUX_DISCONNECTED:
		++cb_data_ptr->event_disconnected;
		cb_data_ptr->event_disconnected_ssr =
			((struct smux_meta_disconnected *)metadata)->is_ssr;
		break;

	case SMUX_READ_DONE:
		++cb_data_ptr->event_read_done;
		read_event_meta = kmalloc(sizeof(struct mock_read_event),
						GFP_ATOMIC);
		if (read_event_meta) {
			read_event_meta->meta =
				*(struct smux_meta_read *)metadata;
			list_add_tail(&read_event_meta->list,
						&cb_data_ptr->read_events);
		}
		break;

	case SMUX_READ_FAIL:
		++cb_data_ptr->event_read_failed;
		read_event_meta = kmalloc(sizeof(struct mock_read_event),
						GFP_ATOMIC);
		if (read_event_meta) {
			read_event_meta->meta =
					*(struct smux_meta_read *)metadata;
			list_add_tail(&read_event_meta->list,
					&cb_data_ptr->read_events);
		}
		break;

	case SMUX_WRITE_DONE:
		++cb_data_ptr->event_write_done;
		write_event_meta = kmalloc(sizeof(struct mock_write_event),
						GFP_ATOMIC);
		if (write_event_meta) {
			write_event_meta->meta =
					*(struct smux_meta_write *)metadata;
			list_add_tail(&write_event_meta->list,
					&cb_data_ptr->write_events);
		}
		break;

	case SMUX_WRITE_FAIL:
		++cb_data_ptr->event_write_failed;
		write_event_meta = kmalloc(sizeof(struct mock_write_event),
						GFP_ATOMIC);
		if (write_event_meta) {
			write_event_meta->meta =
				*(struct smux_meta_write *)metadata;
			list_add_tail(&write_event_meta->list,
					&cb_data_ptr->write_events);
		}
		break;

	case SMUX_LOW_WM_HIT:
		++cb_data_ptr->event_low_wm;
		break;

	case SMUX_HIGH_WM_HIT:
		++cb_data_ptr->event_high_wm;
		break;

	case SMUX_TIOCM_UPDATE:
		++cb_data_ptr->event_tiocm;
		cb_data_ptr->tiocm_meta = *(struct smux_meta_tiocm *)metadata;
		break;

	default:
		pr_err("%s: unknown event %d\n", __func__, event);
	};

	++cb_data_ptr->cb_count;
	complete(&cb_data_ptr->cb_completion);
	spin_unlock_irqrestore(&cb_data_ptr->lock, flags);
}

/**
 * Test Read/write usage.
 *
 * @buf       Output buffer for failure/status messages
 * @max       Size of @buf
 * @vectors   Test vector data (must end with NULL item)
 * @name      Name of the test case for failure messages
 *
 * Perform a sanity test consisting of opening a port, writing test packet(s),
 * reading the response(s), and closing the port.
 *
 * The port should already be configured to use either local or remote
 * loopback.
 */
static int smux_ut_basic_core(char *buf, int max,
	const struct test_vector *vectors,
	const char *name)
{
	int i = 0;
	int failed = 0;
	static struct smux_mock_callback cb_data;
	static int cb_initialized;
	int ret;

	if (!cb_initialized)
		mock_cb_data_init(&cb_data);

	mock_cb_data_reset(&cb_data);
	while (!failed) {
		struct mock_write_event *write_event;
		struct mock_read_event *read_event;

		/* open port */
		ret = msm_smux_open(SMUX_TEST_LCID, &cb_data, smux_mock_cb,
					get_rx_buffer);
		UT_ASSERT_INT(ret, ==, 0);
		UT_ASSERT_INT(
			(int)wait_for_completion_timeout(
					&cb_data.cb_completion, HZ), >, 0);
		UT_ASSERT_INT(cb_data.cb_count, ==, 1);
		UT_ASSERT_INT(cb_data.event_connected, ==, 1);
		mock_cb_data_reset(&cb_data);

		/* write, read, and verify the test vector data */
		for (; vectors->data != NULL; ++vectors) {
			const char *test_data = vectors->data;
			const unsigned test_len = vectors->len;

			i += scnprintf(buf + i, max - i,
					"Writing vector %p len %d\n",
					test_data, test_len);

			/* write data */
			msm_smux_write(SMUX_TEST_LCID, (void *)0xCAFEFACE,
					test_data, test_len);
			UT_ASSERT_INT(ret, ==, 0);
			UT_ASSERT_INT(
					(int)wait_for_completion_timeout(
					&cb_data.cb_completion, HZ), >, 0);

			/* wait for write and echo'd read to complete */
			INIT_COMPLETION(cb_data.cb_completion);
			if (cb_data.cb_count < 2)
				UT_ASSERT_INT(
					(int)wait_for_completion_timeout(
						&cb_data.cb_completion, HZ),
					>, 0);

			UT_ASSERT_INT(cb_data.cb_count, >=, 1);
			UT_ASSERT_INT(cb_data.event_write_done, ==, 1);
			UT_ASSERT_INT(list_empty(&cb_data.write_events), ==, 0);

			write_event = list_first_entry(&cb_data.write_events,
					struct mock_write_event, list);
			UT_ASSERT_PTR(write_event->meta.pkt_priv, ==,
							(void *)0xCAFEFACE);
			UT_ASSERT_PTR(write_event->meta.buffer, ==,
							(void *)test_data);
			UT_ASSERT_INT(write_event->meta.len, ==, test_len);

			/* verify read event */
			UT_ASSERT_INT(cb_data.event_read_done, ==, 1);
			UT_ASSERT_INT(list_empty(&cb_data.read_events), ==, 0);
			read_event = list_first_entry(&cb_data.read_events,
					struct mock_read_event, list);
			UT_ASSERT_PTR(read_event->meta.pkt_priv, ==,
							(void *)0x1234);
			UT_ASSERT_PTR(read_event->meta.buffer, !=, NULL);

			if (read_event->meta.len != test_len ||
				memcmp(read_event->meta.buffer,
						test_data, test_len)) {
				/* data mismatch */
				char linebuff[80];

				hex_dump_to_buffer(test_data, test_len,
					16, 1, linebuff, sizeof(linebuff), 1);
				i += scnprintf(buf + i, max - i,
						"Expected:\n%s\n\n", linebuff);

				hex_dump_to_buffer(read_event->meta.buffer,
					read_event->meta.len,
					16, 1, linebuff, sizeof(linebuff), 1);
				i += scnprintf(buf + i, max - i,
						"Actual:\n%s\n", linebuff);
				failed = 1;
				break;
			}
			mock_cb_data_reset(&cb_data);
		}

		/* close port */
		ret = msm_smux_close(SMUX_TEST_LCID);
		UT_ASSERT_INT(ret, ==, 0);
		UT_ASSERT_INT(
			(int)wait_for_completion_timeout(
				&cb_data.cb_completion, HZ),
			>, 0);
		UT_ASSERT_INT(cb_data.cb_count, ==, 1);
		UT_ASSERT_INT(cb_data.event_disconnected, ==, 1);
		UT_ASSERT_INT(cb_data.event_disconnected_ssr, ==, 0);
		break;
	}

	if (!failed) {
		i += scnprintf(buf + i, max - i, "\tOK\n");
	} else {
		pr_err("%s: Failed\n", name);
		i += scnprintf(buf + i, max - i, "\tFailed\n");
		i += mock_cb_data_print(&cb_data, buf + i, max - i);
		msm_smux_close(SMUX_TEST_LCID);
	}

	mock_cb_data_reset(&cb_data);
	return i;
}

/**
 * Verify Basic Local Loopback Support
 *
 * Perform a sanity test consisting of opening a port in local loopback
 * mode and writing a packet and reading the echo'd packet back.
 */
static int smux_ut_basic(char *buf, int max)
{
	const struct test_vector test_data[] = {
		{"hello\0world\n", sizeof("hello\0world\n")},
		{0, 0},
	};
	int i = 0;
	int failed = 0;
	int ret;

	i += scnprintf(buf + i, max - i, "Running %s\n", __func__);
	while (!failed) {
		/* enable loopback mode */
		ret = msm_smux_set_ch_option(SMUX_TEST_LCID,
				SMUX_CH_OPTION_LOCAL_LOOPBACK, 0);
		UT_ASSERT_INT(ret, ==, 0);

		i += smux_ut_basic_core(buf + i, max - i, test_data, __func__);
		break;
	}

	if (failed) {
		pr_err("%s: Failed\n", __func__);
		i += scnprintf(buf + i, max - i, "\tFailed\n");
	}
	return i;
}

/**
 * Verify Basic Remote Loopback Support
 *
 * Perform a sanity test consisting of opening a port in remote loopback
 * mode and writing a packet and reading the echo'd packet back.
 */
static int smux_ut_remote_basic(char *buf, int max)
{
	const struct test_vector test_data[] = {
		{"hello\0world\n", sizeof("hello\0world\n")},
		{0, 0},
	};
	int i = 0;
	int failed = 0;
	int ret;

	i += scnprintf(buf + i, max - i, "Running %s\n", __func__);
	while (!failed) {
		/* enable remote mode */
		ret = msm_smux_set_ch_option(SMUX_TEST_LCID,
				SMUX_CH_OPTION_REMOTE_LOOPBACK, 0);
		UT_ASSERT_INT(ret, ==, 0);

		i += smux_ut_basic_core(buf + i, max - i, test_data, __func__);
		break;
	}

	if (failed) {
		pr_err("%s: Failed\n", __func__);
		i += scnprintf(buf + i, max - i, "\tFailed\n");
	}
	return i;
}

/**
 * Fill test pattern into provided buffer including an optional
 * redzone 16 bytes before and 16 bytes after the buffer.
 *
 * buf ---------
 *      redzone
 *     --------- <- returned pointer
 *       data
 *     --------- <- returned pointer + len
 *      redzone
 *     ---------
 *
 * @buf  Pointer to the buffer of size len or len+32 (redzone)
 * @len  Length of the *data* buffer (excluding 32-byte redzone)
 * @redzone If true, adds redzone data
 *
 * @returns pointer to buffer (buf + 16 if redzone enabled)
 */
uint8_t *test_pattern_fill(char *buf, int len, int redzone)
{
	void *ret;
	uint8_t ch;

	ret = buf;
	if (redzone) {
		memset((char *)buf, 0xAB, 16);
		memset((char *)buf + len, 0xBA, 16);
		ret += 16;
	}

	/* fill with test pattern */
	for (ch = 0; len > 0; --len, ++ch)
		*buf++ = (char)ch;

	return ret;
}

/**
 * Verify test pattern generated by test_pattern_fill.
 *
 * @buf_ptr    Pointer to buffer pointer
 * @len        Length of the *data* buffer (excluding 32-byte redzone)
 * @redzone    If true, verifies redzone and adjusts *buf_ptr
 * @errmsg     Buffer for error message
 * @errmsg_max Size of error message buffer
 *
 * @returns    0 for success; length of error message otherwise
 */
unsigned test_pattern_verify(char **buf_ptr, int len, int redzone,
					char *errmsg, int errmsg_max)
{
	int n;
	int i = 0;
	char linebuff[80];

	if (redzone) {
		*buf_ptr -= 16;

		/* verify prefix redzone */
		for (n = 0; n < 16; ++n) {
			if (*buf_ptr[n] != 0xAB) {
				hex_dump_to_buffer(*buf_ptr, 16,
					16, 1, linebuff, sizeof(linebuff), 1);
				i += scnprintf(errmsg + i, errmsg_max - i,
					"Redzone violation: %s\n", linebuff);
				break;
			}
		}

		/* verify postfix redzone */
		for (n = 0; n < 16; ++n) {
			if (*buf_ptr[len + n] != 0xBA) {
				hex_dump_to_buffer(&(*buf_ptr)[len], 16,
					16, 1, linebuff, sizeof(linebuff), 1);
				i += scnprintf(errmsg + i, errmsg_max - i,
					"Redzone violation: %s\n", linebuff);
				break;
			}
		}
	}
	return i;
}

/**
 * Write a multiple packets in ascending size and verify packet is received
 * correctly.
 *
 * @buf  Buffer for status message
 * @max  Size of buffer
 * @name Name of the test for error reporting
 *
 * @returns Number of bytes written to @buf
 *
 * Requires that the port already be opened and loopback mode is
 * configured correctly (if required).
 */
static int smux_ut_loopback_big_pkt(char *buf, int max, const char *name)
{
	struct test_vector test_data[] = {
		{0, 64},
		{0, 128},
		{0, 256},
		{0, 512},
		{0, 1024},
		{0, 2048},
		{0, 4096},
		{0, 0},
	};
	int i = 0;
	int failed = 0;
	struct test_vector *tv;

	/* generate test data */
	for (tv = test_data; tv->len > 0; ++tv) {
		tv->data = kmalloc(tv->len + 32, GFP_KERNEL);
		pr_err("%s: allocating %p len %d\n",
				__func__, tv->data, tv->len);
		if (!tv->data) {
			i += scnprintf(buf + i, max - i,
					"%s: Unable to allocate %d bytes\n",
					__func__, tv->len);
			failed = 1;
			goto out;
		}
		test_pattern_fill((uint8_t *)tv->data, tv->len, 1);
	}

	/* run test */
	i += scnprintf(buf + i, max - i, "Running %s\n", name);
	while (!failed) {
		i += smux_ut_basic_core(buf + i, max - i, test_data, name);
		break;
	}

out:
	if (failed) {
		pr_err("%s: Failed\n", name);
		i += scnprintf(buf + i, max - i, "\tFailed\n");
	}

	for (tv = test_data; tv->len > 0; ++tv) {
		if (!tv->data) {
			i += test_pattern_verify((char **)&tv->data,
						tv->len, 1, buf + i, max - i);
			pr_err("%s: freeing %p len %d\n", __func__,
							tv->data, tv->len);
			kfree(tv->data);
		}
	}

	return i;
}

/**
 * Verify Large-packet Local Loopback Support.
 *
 * @buf  Buffer for status message
 * @max  Size of buffer
 *
 * @returns Number of bytes written to @buf
 *
 * Open port in local loopback mode and write a multiple packets in ascending
 * size and verify packet is received correctly.
 */
static int smux_ut_local_big_pkt(char *buf, int max)
{
	int i = 0;
	int ret;

	ret = msm_smux_set_ch_option(SMUX_TEST_LCID,
			SMUX_CH_OPTION_LOCAL_LOOPBACK, 0);

	if (ret == 0) {
		smux_byte_loopback = SMUX_TEST_LCID;
		i += smux_ut_loopback_big_pkt(buf, max, __func__);
		smux_byte_loopback = 0;
	} else {
		i += scnprintf(buf + i, max - i,
				"%s: Unable to set loopback mode\n",
				__func__);
	}

	return i;
}

/**
 * Verify Large-packet Remote Loopback Support.
 *
 * @buf  Buffer for status message
 * @max  Size of buffer
 *
 * @returns Number of bytes written to @buf
 *
 * Open port in remote loopback mode and write a multiple packets in ascending
 * size and verify packet is received correctly.
 */
static int smux_ut_remote_big_pkt(char *buf, int max)
{
	int i = 0;
	int ret;

	ret = msm_smux_set_ch_option(SMUX_TEST_LCID,
			SMUX_CH_OPTION_REMOTE_LOOPBACK, 0);
	if (ret == 0) {
		i += smux_ut_loopback_big_pkt(buf, max, __func__);
	} else {
		i += scnprintf(buf + i, max - i,
				"%s: Unable to set loopback mode\n",
				__func__);
	}

	return i;
}

/**
 * Verify set and get operations for each TIOCM bit.
 *
 * @buf  Buffer for status message
 * @max  Size of buffer
 * @name Name of the test for error reporting
 *
 * @returns Number of bytes written to @buf
 */
static int smux_ut_tiocm(char *buf, int max, const char *name)
{
	static struct smux_mock_callback cb_data;
	static int cb_initialized;
	static const struct tiocm_test_vector tiocm_vectors[] = {
		/* bit to set, set old, set new, clear old */
		{TIOCM_DTR, TIOCM_DTR, TIOCM_DTR | TIOCM_DSR, TIOCM_DSR},
		{TIOCM_RTS, TIOCM_RTS, TIOCM_RTS | TIOCM_CTS, TIOCM_CTS},
		{TIOCM_RI, 0x0, TIOCM_RI, TIOCM_RI},
		{TIOCM_CD, 0x0, TIOCM_CD, TIOCM_CD},
	};
	int i = 0;
	int failed = 0;
	int n;
	int ret;

	i += scnprintf(buf + i, max - i, "Running %s\n", name);

	if (!cb_initialized)
		mock_cb_data_init(&cb_data);

	mock_cb_data_reset(&cb_data);
	while (!failed) {
		/* open port */
		ret = msm_smux_open(SMUX_TEST_LCID, &cb_data, smux_mock_cb,
								get_rx_buffer);
		UT_ASSERT_INT(ret, ==, 0);
		UT_ASSERT_INT(
				(int)wait_for_completion_timeout(
					&cb_data.cb_completion, HZ), >, 0);
		UT_ASSERT_INT(cb_data.cb_count, ==, 1);
		UT_ASSERT_INT(cb_data.event_connected, ==, 1);
		mock_cb_data_reset(&cb_data);

		/* set and clear each TIOCM bit */
		for (n = 0; n < ARRAY_SIZE(tiocm_vectors) && !failed; ++n) {
			/* set signal and verify */
			ret = msm_smux_tiocm_set(SMUX_TEST_LCID,
						tiocm_vectors[n].input, 0x0);
			UT_ASSERT_INT(ret, ==, 0);
			UT_ASSERT_INT(
				(int)wait_for_completion_timeout(
					&cb_data.cb_completion, HZ), >, 0);
			UT_ASSERT_INT(cb_data.cb_count, ==, 1);
			UT_ASSERT_INT(cb_data.event_tiocm, ==, 1);
			UT_ASSERT_INT(cb_data.tiocm_meta.tiocm_old, ==,
						tiocm_vectors[n].set_old);
			UT_ASSERT_INT(cb_data.tiocm_meta.tiocm_new, ==,
						tiocm_vectors[n].set_new);
			mock_cb_data_reset(&cb_data);

			/* clear signal and verify */
			ret = msm_smux_tiocm_set(SMUX_TEST_LCID, 0x0,
						tiocm_vectors[n].input);
			UT_ASSERT_INT(ret, ==, 0);
			UT_ASSERT_INT(
				(int)wait_for_completion_timeout(
					&cb_data.cb_completion, HZ),
				>, 0);
			UT_ASSERT_INT(cb_data.cb_count, ==, 1);
			UT_ASSERT_INT(cb_data.event_tiocm, ==, 1);
			UT_ASSERT_INT(cb_data.tiocm_meta.tiocm_old, ==,
						tiocm_vectors[n].clr_old);
			UT_ASSERT_INT(cb_data.tiocm_meta.tiocm_new, ==, 0x0);
			mock_cb_data_reset(&cb_data);
		}
		if (failed)
			break;

		/* close port */
		ret = msm_smux_close(SMUX_TEST_LCID);
		UT_ASSERT_INT(ret, ==, 0);
		UT_ASSERT_INT(
			(int)wait_for_completion_timeout(
				&cb_data.cb_completion, HZ),
			>, 0);
		UT_ASSERT_INT(cb_data.cb_count, ==, 1);
		UT_ASSERT_INT(cb_data.event_disconnected, ==, 1);
		UT_ASSERT_INT(cb_data.event_disconnected_ssr, ==, 0);
		break;
	}

	if (!failed) {
		i += scnprintf(buf + i, max - i, "\tOK\n");
	} else {
		pr_err("%s: Failed\n", name);
		i += scnprintf(buf + i, max - i, "\tFailed\n");
		i += mock_cb_data_print(&cb_data, buf + i, max - i);
		msm_smux_close(SMUX_TEST_LCID);
	}

	mock_cb_data_reset(&cb_data);
	return i;
}

/**
 * Verify TIOCM Status Bits for local loopback.
 *
 * @buf  Buffer for status message
 * @max  Size of buffer
 *
 * @returns Number of bytes written to @buf
 */
static int smux_ut_local_tiocm(char *buf, int max)
{
	int i = 0;
	int ret;

	ret = msm_smux_set_ch_option(SMUX_TEST_LCID,
			SMUX_CH_OPTION_LOCAL_LOOPBACK, 0);

	if (ret == 0) {
		smux_byte_loopback = SMUX_TEST_LCID;
		i += smux_ut_tiocm(buf, max, __func__);
		smux_byte_loopback = 0;
	} else {
		i += scnprintf(buf + i, max - i,
				"%s: Unable to set loopback mode\n",
				__func__);
	}

	return i;
}

/**
 * Verify TIOCM Status Bits for remote loopback.
 *
 * @buf  Buffer for status message
 * @max  Size of buffer
 *
 * @returns Number of bytes written to @buf
 */
static int smux_ut_remote_tiocm(char *buf, int max)
{
	int i = 0;
	int ret;

	ret = msm_smux_set_ch_option(SMUX_TEST_LCID,
			SMUX_CH_OPTION_REMOTE_LOOPBACK, 0);
	if (ret == 0) {
		i += smux_ut_tiocm(buf, max, __func__);
	} else {
		i += scnprintf(buf + i, max - i,
				"%s: Unable to set loopback mode\n",
				__func__);
	}

	return i;
}

/**
 * Verify High/Low Watermark notifications.
 *
 * @buf  Buffer for status message
 * @max  Size of buffer
 *
 * @returns Number of bytes written to @buf
 */
static int smux_ut_local_wm(char *buf, int max)
{
	static struct smux_mock_callback cb_data;
	static int cb_initialized;
	int i = 0;
	int failed = 0;
	int ret;

	i += scnprintf(buf + i, max - i, "Running %s\n", __func__);
	pr_err("%s", buf);

	if (!cb_initialized)
		mock_cb_data_init(&cb_data);

	mock_cb_data_reset(&cb_data);
	smux_byte_loopback = SMUX_TEST_LCID;
	while (!failed) {
		/* open port for loopback with TX disabled */
		ret = msm_smux_set_ch_option(SMUX_TEST_LCID,
				SMUX_CH_OPTION_LOCAL_LOOPBACK
				| SMUX_CH_OPTION_REMOTE_TX_STOP,
				0);
		UT_ASSERT_INT(ret, ==, 0);

		ret = msm_smux_open(SMUX_TEST_LCID, &cb_data, smux_mock_cb,
								get_rx_buffer);
		UT_ASSERT_INT(ret, ==, 0);
		UT_ASSERT_INT(
				(int)wait_for_completion_timeout(
					&cb_data.cb_completion, HZ), >, 0);
		UT_ASSERT_INT(cb_data.cb_count, ==, 1);
		UT_ASSERT_INT(cb_data.event_connected, ==, 1);
		mock_cb_data_reset(&cb_data);

		/* transmit 4 packets and verify high-watermark notification */
		ret = 0;
		ret |= msm_smux_write(SMUX_TEST_LCID, (void *)1,
					test_array, sizeof(test_array));
		ret |= msm_smux_write(SMUX_TEST_LCID, (void *)2,
					test_array, sizeof(test_array));
		ret |= msm_smux_write(SMUX_TEST_LCID, (void *)3,
					test_array, sizeof(test_array));
		UT_ASSERT_INT(ret, ==, 0);
		UT_ASSERT_INT(cb_data.cb_count, ==, 0);
		UT_ASSERT_INT(cb_data.event_high_wm, ==, 0);

		ret = msm_smux_write(SMUX_TEST_LCID, (void *)4,
					test_array, sizeof(test_array));
		UT_ASSERT_INT(ret, ==, 0);
		UT_ASSERT_INT(
			(int)wait_for_completion_timeout(
				&cb_data.cb_completion, HZ),
			>, 0);
		UT_ASSERT_INT(cb_data.event_high_wm, ==, 1);
		UT_ASSERT_INT(cb_data.event_low_wm, ==, 0);
		mock_cb_data_reset(&cb_data);

		/* exceed watermark and verify failure return value */
		ret = msm_smux_write(SMUX_TEST_LCID, (void *)5,
					test_array, sizeof(test_array));
		UT_ASSERT_INT(ret, ==, -EAGAIN);

		/* re-enable TX and verify low-watermark notification */
		ret = msm_smux_set_ch_option(SMUX_TEST_LCID,
				0, SMUX_CH_OPTION_REMOTE_TX_STOP);
		UT_ASSERT_INT(ret, ==, 0);
		while (cb_data.cb_count < 9) {
			UT_ASSERT_INT(
				(int)wait_for_completion_timeout(
					&cb_data.cb_completion, HZ),
				>, 0);
			INIT_COMPLETION(cb_data.cb_completion);
		}
		if (failed)
			break;

		UT_ASSERT_INT(cb_data.event_high_wm, ==, 0);
		UT_ASSERT_INT(cb_data.event_low_wm, ==, 1);
		UT_ASSERT_INT(cb_data.event_write_done, ==, 4);
		mock_cb_data_reset(&cb_data);

		/* close port */
		ret = msm_smux_close(SMUX_TEST_LCID);
		UT_ASSERT_INT(ret, ==, 0);
		UT_ASSERT_INT(
			(int)wait_for_completion_timeout(
				&cb_data.cb_completion, HZ),
			>, 0);
		UT_ASSERT_INT(cb_data.cb_count, ==, 1);
		UT_ASSERT_INT(cb_data.event_disconnected, ==, 1);
		UT_ASSERT_INT(cb_data.event_disconnected_ssr, ==, 0);
		break;
	}

	if (!failed) {
		i += scnprintf(buf + i, max - i, "\tOK\n");
	} else {
		pr_err("%s: Failed\n", __func__);
		i += scnprintf(buf + i, max - i, "\tFailed\n");
		i += mock_cb_data_print(&cb_data, buf + i, max - i);
		msm_smux_close(SMUX_TEST_LCID);
	}
	smux_byte_loopback = 0;
	mock_cb_data_reset(&cb_data);
	return i;
}

/**
 * Verify smuxld_receive_buf regular and error processing.
 *
 * @buf  Buffer for status message
 * @max  Size of buffer
 *
 * @returns Number of bytes written to @buf
 */
static int smux_ut_local_smuxld_receive_buf(char *buf, int max)
{
	static struct smux_mock_callback cb_data;
	static int cb_initialized;
	struct mock_read_event *meta;
	int i = 0;
	int failed = 0;
	int ret;
	char data[] = {SMUX_UT_ECHO_REQ,
		SMUX_UT_ECHO_REQ, SMUX_UT_ECHO_REQ,
	};
	char flags[] = {0x0, 0x1, 0x0,};


	i += scnprintf(buf + i, max - i, "Running %s\n", __func__);

	if (!cb_initialized)
		mock_cb_data_init(&cb_data);

	mock_cb_data_reset(&cb_data);
	smux_byte_loopback = SMUX_TEST_LCID;
	while (!failed) {
		/* open port for loopback */
		ret = msm_smux_set_ch_option(SMUX_TEST_LCID,
				SMUX_CH_OPTION_LOCAL_LOOPBACK, 0);
		UT_ASSERT_INT(ret, ==, 0);

		ret = msm_smux_open(SMUX_TEST_LCID, &cb_data, smux_mock_cb,
								get_rx_buffer);
		UT_ASSERT_INT(ret, ==, 0);
		UT_ASSERT_INT(
				(int)wait_for_completion_timeout(
					&cb_data.cb_completion, HZ), >, 0);
		UT_ASSERT_INT(cb_data.cb_count, ==, 1);
		UT_ASSERT_INT(cb_data.event_connected, ==, 1);
		mock_cb_data_reset(&cb_data);

		/*
		 * Verify RX error processing by sending 3 echo requests:
		 *     one OK, one fail, and a final OK
		 *
		 * The parsing framework should process the requests
		 * and send us three BYTE command packets with
		 * ECHO ACK FAIL and ECHO ACK OK characters.
		 */
		smuxld_receive_buf(0, data, flags, sizeof(data));

		/* verify response characters */
		do {
			UT_ASSERT_INT(
				(int)wait_for_completion_timeout(
					&cb_data.cb_completion, HZ), >, 0);
			INIT_COMPLETION(cb_data.cb_completion);
		} while (cb_data.cb_count < 3);
		UT_ASSERT_INT(cb_data.cb_count, ==, 3);
		UT_ASSERT_INT(cb_data.event_read_done, ==, 3);

		meta = list_first_entry(&cb_data.read_events,
				struct mock_read_event, list);
		UT_ASSERT_INT((int)meta->meta.pkt_priv, ==,
				SMUX_UT_ECHO_ACK_OK);
		list_del(&meta->list);

		meta = list_first_entry(&cb_data.read_events,
				struct mock_read_event, list);
		UT_ASSERT_INT((int)meta->meta.pkt_priv, ==,
				SMUX_UT_ECHO_ACK_FAIL);
		list_del(&meta->list);

		meta = list_first_entry(&cb_data.read_events,
				struct mock_read_event, list);
		UT_ASSERT_INT((int)meta->meta.pkt_priv, ==,
				SMUX_UT_ECHO_ACK_OK);
		list_del(&meta->list);
		mock_cb_data_reset(&cb_data);

		/* close port */
		ret = msm_smux_close(SMUX_TEST_LCID);
		UT_ASSERT_INT(ret, ==, 0);
		UT_ASSERT_INT(
			(int)wait_for_completion_timeout(
				&cb_data.cb_completion, HZ),
			>, 0);
		UT_ASSERT_INT(cb_data.cb_count, ==, 1);
		UT_ASSERT_INT(cb_data.event_disconnected, ==, 1);
		UT_ASSERT_INT(cb_data.event_disconnected_ssr, ==, 0);
		break;
	}

	if (!failed) {
		i += scnprintf(buf + i, max - i, "\tOK\n");
	} else {
		pr_err("%s: Failed\n", __func__);
		i += scnprintf(buf + i, max - i, "\tFailed\n");
		i += mock_cb_data_print(&cb_data, buf + i, max - i);
		msm_smux_close(SMUX_TEST_LCID);
	}
	smux_byte_loopback = 0;
	mock_cb_data_reset(&cb_data);
	return i;
}

static char debug_buffer[DEBUG_BUFMAX];

static ssize_t debug_read(struct file *file, char __user *buf,
			  size_t count, loff_t *ppos)
{
	int (*fill)(char *buf, int max) = file->private_data;
	int bsize;

	if (*ppos != 0)
		return 0;

	bsize = fill(debug_buffer, DEBUG_BUFMAX);
	return simple_read_from_buffer(buf, count, ppos, debug_buffer, bsize);
}

static int debug_open(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;
	return 0;
}

static const struct file_operations debug_ops = {
	.read = debug_read,
	.open = debug_open,
};

static void debug_create(const char *name, mode_t mode,
			 struct dentry *dent,
			 int (*fill)(char *buf, int max))
{
	debugfs_create_file(name, mode, dent, fill, &debug_ops);
}

static int __init smux_debugfs_init(void)
{
	struct dentry *dent;

	dent = debugfs_create_dir("n_smux", 0);
	if (IS_ERR(dent))
		return PTR_ERR(dent);

	/*
	 * Add Unit Test entries.
	 *
	 * The idea with unit tests is that you can run all of them
	 * from ADB shell by doing:
	 *  adb shell
	 *	cat ut*
	 *
	 * And if particular tests fail, you can then repeatedly run the failing
	 * tests as you debug and resolve the failing test.
	 */
	debug_create("ut_local_basic", 0444, dent, smux_ut_basic);
	debug_create("ut_remote_basic", 0444, dent,	smux_ut_remote_basic);
	debug_create("ut_local_big_pkt", 0444, dent, smux_ut_local_big_pkt);
	debug_create("ut_remote_big_pkt", 0444, dent, smux_ut_remote_big_pkt);
	debug_create("ut_local_tiocm", 0444, dent, smux_ut_local_tiocm);
	debug_create("ut_remote_tiocm", 0444, dent,	smux_ut_remote_tiocm);
	debug_create("ut_local_wm", 0444, dent, smux_ut_local_wm);
	debug_create("ut_local_smuxld_receive_buf", 0444, dent,
			smux_ut_local_smuxld_receive_buf);

	return 0;
}

late_initcall(smux_debugfs_init);

