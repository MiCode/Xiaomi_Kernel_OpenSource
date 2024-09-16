// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
/*******************************************************************************
*                    E X T E R N A L   R E F E R E N C E S
********************************************************************************
*/
#include <linux/spinlock.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/workqueue.h>
#include <linux/ratelimit.h>
#include <linux/alarmtimer.h>
#include <linux/suspend.h>

#include "connsyslog.h"
#include "connsyslog_emi.h"
#include "connsyslog_hw_config.h"
#include "ring.h"
#include "ring_emi.h"

/*******************************************************************************
*                             D A T A   T Y P E S
********************************************************************************
*/

/* Close debug log */
//#define DEBUG_RING 1

#define CONNLOG_ALARM_STATE_DISABLE	0x0
#define CONNLOG_ALARM_STATE_ENABLE	0x01
#define CONNLOG_ALARM_STATE_RUNNING	0x03

#define BYETES_PER_LINE 16
#define LOG_LINE_SIZE (3*BYETES_PER_LINE + BYETES_PER_LINE + 1)
#define IS_VISIBLE_CHAR(c) ((c) >= 32 && (c) <= 126)

#define LOG_MAX_LEN 1024
#define LOG_HEAD_LENG 16
#define TIMESYNC_LENG 40

static const char log_head[] = {0x55, 0x00, 0x00, 0x62};
static const char timesync_head[] = {0x55, 0x00, 0x25, 0x62};

struct connlog_alarm {
	struct alarm alarm_timer;
	unsigned int alarm_state;
	unsigned int blank_state;
	unsigned int alarm_sec;
	spinlock_t alarm_lock;
	unsigned long flags;
};

struct connlog_offset {
	unsigned int emi_base_offset;
	unsigned int emi_size;
	unsigned int emi_read;
	unsigned int emi_write;
	unsigned int emi_buf;
	unsigned int emi_guard_pattern_offset;
};

struct connlog_buffer {
	struct ring_emi ring_emi;
	struct ring ring_cache;
	void *cache_base;
};

struct connlog_event_cb {
       CONNLOG_EVENT_CB log_data_handler;
};

struct connlog_dev {
	int conn_type;
	phys_addr_t phyAddrEmiBase;
	unsigned int emi_size;
	void __iomem *virAddrEmiLogBase;
	struct connlog_offset log_offset;
	struct connlog_buffer log_buffer;
	bool eirqOn;
	spinlock_t irq_lock;
	unsigned long flags;
	unsigned int irq_counter;
	struct timer_list workTimer;
	struct work_struct logDataWorker;
	void *log_data;
	char log_line[LOG_MAX_LEN];
	struct connlog_event_cb callback;
};

static char *type_to_title[CONN_DEBUG_TYPE_END] = {
	"wifi_fw", "bt_fw"
};

static struct connlog_dev* gLogDev[CONN_DEBUG_TYPE_END];

#ifdef CONFIG_MTK_CONNSYS_DEDICATED_LOG_PATH
static atomic_t g_log_mode = ATOMIC_INIT(LOG_TO_FILE);
#else
static atomic_t g_log_mode = ATOMIC_INIT(PRINT_TO_KERNEL_LOG);
#endif

static phys_addr_t gPhyEmiBase;

/* alarm timer for suspend */
struct connlog_alarm gLogAlarm;

/*******************************************************************************
*                  F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/
static void connlog_do_schedule_work(struct connlog_dev* handler, bool count);
static void work_timer_handler(unsigned long data);
static void connlog_event_set(struct connlog_dev* handler);
static struct connlog_dev* connlog_subsys_init(
	int conn_type,
	phys_addr_t emiaddr,
	unsigned int emi_size);
static void connlog_subsys_deinit(struct connlog_dev* handler);
static ssize_t connlog_read_internal(
	struct connlog_dev* handler, int conn_type,
	char *buf, char __user *userbuf, size_t count, bool to_user);
static void connlog_dump_emi(struct connlog_dev* handler, int offset, int size);

/*******************************************************************************
*                              F U N C T I O N S
********************************************************************************
*/

struct connlog_emi_config* __weak get_connsyslog_platform_config(int conn_type)
{
	pr_err("Miss platform ops !!\n");
	return NULL;
}

void *connlog_cache_allocate(size_t size)
{
	void *pBuffer = NULL;

	if (size > (PAGE_SIZE << 1))
		pBuffer = vmalloc(size);
	else
	pBuffer = kmalloc(size, GFP_KERNEL);

	/* If there is fragment, kmalloc may not get memory when size > one page.
	 * For this case, use vmalloc instead.
	 */
	if (pBuffer == NULL && size > PAGE_SIZE)
		pBuffer = vmalloc(size);

	return pBuffer;
}

void connlog_cache_free(const void *dst)
{
	kvfree(dst);
}

/*****************************************************************************
 * FUNCTION
 *  connlog_event_set
 * DESCRIPTION
 *  Trigger  event call back to wakeup waitqueue
 * PARAMETERS
 *  conn_type      [IN]        subsys type
 * RETURNS
 *  void
 *****************************************************************************/
static void connlog_event_set(struct connlog_dev* handler)
{
	if (handler->callback.log_data_handler)
		handler->callback.log_data_handler();
}


/*****************************************************************************
* FUNCTION
*  connlog_set_ring_ready
* DESCRIPTION
*  set reserved bit be EMIFWLOG to indicate that init is ready.
* PARAMETERS
*  void
* RETURNS
*  void
*****************************************************************************/
static void connlog_set_ring_ready(struct connlog_dev* handler)
{
	const char ready_str[] = "EMIFWLOG";

	memcpy_toio(handler->virAddrEmiLogBase + CONNLOG_READY_PATTERN_BASE,
		    ready_str, CONNLOG_READY_PATTERN_BASE_SIZE);
}

static unsigned int connlog_cal_log_size(unsigned int emi_size)
{
	int position;
	int i;

	if (emi_size > 0) {
		for (i = (emi_size >> 1), position = 0; i != 0; ++position)
			i >>= 1;
	} else {
		return 0;
	}

	return (1UL << position);
}

static int connlog_emi_init(struct connlog_dev* handler, phys_addr_t emiaddr, unsigned int emi_size)
{
	int conn_type = handler->conn_type;
	unsigned int cal_log_size = connlog_cal_log_size(
		emi_size - CONNLOG_EMI_BASE_OFFSET - CONNLOG_EMI_END_PATTERN_SIZE);

	if (emiaddr == 0 || cal_log_size == 0) {
		pr_err("[%s] consys emi memory address invalid emi_addr=%p emi_size=%d\n",
			type_to_title[conn_type], emiaddr, emi_size);
		return -1;
	}
	pr_info("input size = %d cal_size = %d\n", emi_size, cal_log_size);

	handler->phyAddrEmiBase = emiaddr;
	handler->emi_size = emi_size;
	handler->virAddrEmiLogBase = ioremap_nocache(handler->phyAddrEmiBase, emi_size);
	handler->log_offset.emi_base_offset = CONNLOG_EMI_BASE_OFFSET;
	handler->log_offset.emi_size = cal_log_size;
	handler->log_offset.emi_read = CONNLOG_EMI_READ;
	handler->log_offset.emi_write = CONNLOG_EMI_WRITE;
	handler->log_offset.emi_buf = CONNLOG_EMI_BUF;
	handler->log_offset.emi_guard_pattern_offset = handler->log_offset.emi_buf + handler->log_offset.emi_size;

	if (handler->virAddrEmiLogBase) {
		pr_info("[%s] EMI mapping OK virtual(0x%p) physical(0x%x) size=%d\n",
			type_to_title[conn_type],
			handler->virAddrEmiLogBase,
			(unsigned int)handler->phyAddrEmiBase,
			handler->emi_size);
		/* Clean it  */
		memset_io(handler->virAddrEmiLogBase, 0xff, handler->emi_size);
		/* Clean control block as 0 */
		memset_io(handler->virAddrEmiLogBase + CONNLOG_EMI_BASE_OFFSET, 0x0, CONNLOG_EMI_32_BYTE_ALIGNED);
		/* Setup henader */
		EMI_WRITE32(handler->virAddrEmiLogBase + 0, handler->log_offset.emi_base_offset);
		EMI_WRITE32(handler->virAddrEmiLogBase + 4, handler->log_offset.emi_size);
		/* Setup end pattern */
		memcpy_toio(
			handler->virAddrEmiLogBase + handler->log_offset.emi_guard_pattern_offset,
			CONNLOG_EMI_END_PATTERN, CONNLOG_EMI_END_PATTERN_SIZE);
	} else {
		pr_err("[%s] EMI mapping fail\n", type_to_title[conn_type]);
		return -1;
	}

	return 0;
}

/*****************************************************************************
* FUNCTION
*  connlog_emi_deinit
* DESCRIPTION
*  Do iounmap for log buffer on EMI
* PARAMETERS
*  void
* RETURNS
*  void
*****************************************************************************/
static void connlog_emi_deinit(struct connlog_dev* handler)
{
	iounmap(handler->virAddrEmiLogBase);
}

static int connlog_buffer_init(struct connlog_dev* handler)
{
	void *pBuffer = NULL;

	/* Init ring emi */
	ring_emi_init(
		handler->virAddrEmiLogBase + handler->log_offset.emi_buf,
		handler->log_offset.emi_size,
		handler->virAddrEmiLogBase + handler->log_offset.emi_read,
		handler->virAddrEmiLogBase + handler->log_offset.emi_write,
		&handler->log_buffer.ring_emi);

	/* init ring cache */
	/* TODO: use emi size. Need confirm */
	pBuffer = connlog_cache_allocate(handler->emi_size);
	if (pBuffer == NULL) {
		pr_info("[%s] allocate cache fail.", __func__);
		return -ENOMEM;
	}

	handler->log_buffer.cache_base = pBuffer;
	memset(handler->log_buffer.cache_base, 0, handler->emi_size);
	ring_init(
		handler->log_buffer.cache_base,
		handler->log_offset.emi_size,
		0,
		0,
		&handler->log_buffer.ring_cache);

	return 0;
}

static int connlog_ring_buffer_init(struct connlog_dev* handler)
{
	void *pBuffer = NULL;
	if (!handler->virAddrEmiLogBase) {
		pr_err("[%s] consys emi memory address phyAddrEmiBase invalid\n",
			type_to_title[handler->conn_type]);
		return -1;
	}
	connlog_buffer_init(handler);
	/* TODO: use emi size. Need confirm */
	pBuffer = connlog_cache_allocate(handler->emi_size);
	if (pBuffer == NULL) {
		pr_info("[%s] allocate ring buffer fail.", __func__);
		return -ENOMEM;
	}
	handler->log_data = pBuffer;
	connlog_set_ring_ready(handler);
	return 0;
}

/*****************************************************************************
* FUNCTION
*  connlog_ring_buffer_deinit
* DESCRIPTION
*  Initialize ring buffer setting for subsys
* PARAMETERS
*  void
* RETURNS
*  void
*****************************************************************************/
static void connlog_ring_buffer_deinit(struct connlog_dev* handler)
{
	if (handler->log_buffer.cache_base) {
		connlog_cache_free(handler->log_buffer.cache_base);
		handler->log_buffer.cache_base = NULL;
	}

	if (handler->log_data) {
		connlog_cache_free(handler->log_data);
		handler->log_data = NULL;
	}
}

/*****************************************************************************
* FUNCTION
*  work_timer_handler
* DESCRIPTION
*  IRQ is still on, do schedule_work again
* PARAMETERS
*  data      [IN]        input data
* RETURNS
*  void
*****************************************************************************/
static void work_timer_handler(unsigned long data)
{
	struct connlog_dev* handler = (struct connlog_dev*)data;
	connlog_do_schedule_work(handler, false);
}

/*****************************************************************************
* FUNCTION
*  connlog_dump_buf
* DESCRIPTION
*  Dump EMI content. Output format:
* xx xx xx xx xx xx xx xx xx xx xx xx xx xx xx xx ................
* 3 digits hex * 16 + 16 single char + 1 NULL terminate = 64+1 bytes
* PARAMETERS
*
* RETURNS
*  void
*****************************************************************************/
void connsys_log_dump_buf(const char *title, const char *buf, ssize_t sz)
{
	int i;
	char line[LOG_LINE_SIZE];

	i = 0;
	line[LOG_LINE_SIZE-1] = 0;
	while (sz--) {
		snprintf(line + i*3, 3, "%02x", *buf);
		line[i*3 + 2] = ' ';

		if (IS_VISIBLE_CHAR(*buf))
			line[3*BYETES_PER_LINE + i] = *buf;
		else
			line[3*BYETES_PER_LINE + i] = '`';

		i++;
		buf++;

		if (i >= BYETES_PER_LINE || !sz) {
			if (i < BYETES_PER_LINE) {
				memset(line+i*3, ' ', (BYETES_PER_LINE-i)*3);
				memset(line+3*BYETES_PER_LINE+i, '.', BYETES_PER_LINE-i);
			}
			pr_info("%s: %s\n", title, line);
			i = 0;
		}
	}
}
EXPORT_SYMBOL(connsys_log_dump_buf);

/*****************************************************************************
* FUNCTION
*  connlog_dump_emi
* DESCRIPTION
*  dump EMI buffer for debug.
* PARAMETERS
*  offset          [IN]        buffer offset
*  size            [IN]        dump buffer size
* RETURNS
*  void
*****************************************************************************/
void connlog_dump_emi(struct connlog_dev* handler, int offset, int size)
{
	char title[100];
	memset(title, 0, 100);
	sprintf(title, "%s(%p)", "emi", handler->virAddrEmiLogBase + offset);
	connsys_log_dump_buf(title, handler->virAddrEmiLogBase + offset, size);
}

/*****************************************************************************
* FUNCTION
*  connlog_ring_emi_check
* DESCRIPTION
*
* PARAMETERS
*
* RETURNS
*  void
*****************************************************************************/
static bool connlog_ring_emi_check(struct connlog_dev* handler)
{
	struct ring_emi *ring_emi = &handler->log_buffer.ring_emi;
	char line[CONNLOG_EMI_END_PATTERN_SIZE + 1];

	memcpy_fromio(
		line,
		handler->virAddrEmiLogBase + handler->log_offset.emi_guard_pattern_offset,
		CONNLOG_EMI_END_PATTERN_SIZE);
	line[CONNLOG_EMI_END_PATTERN_SIZE] = '\0';

	/* Check ring_emi buffer memory. Dump EMI data if it is corruption. */
	if (EMI_READ32(ring_emi->read) > handler->log_offset.emi_size ||
	    EMI_READ32(ring_emi->write) > handler->log_offset.emi_size ||
	    strncmp(line, CONNLOG_EMI_END_PATTERN, CONNLOG_EMI_END_PATTERN_SIZE) != 0) {
		pr_err("[connlog] %s out of bound or guard pattern overwrited. Read(pos=%p)=[0x%x] write(pos=%p)=[0x%x] size=[0x%x]\n",
			type_to_title[handler->conn_type],
			ring_emi->read, EMI_READ32(ring_emi->read),
			ring_emi->write, EMI_READ32(ring_emi->write),
			handler->log_offset.emi_size);
		connlog_dump_emi(handler, 0x0, 0x60);
		connlog_dump_emi(handler, CONNLOG_EMI_BASE_OFFSET, 0x20);
		connlog_dump_emi(
			handler,
			handler->log_offset.emi_guard_pattern_offset,
			CONNLOG_EMI_END_PATTERN_SIZE);
		return false;
	}

	return true;
}

/*****************************************************************************
* FUNCTION
*  connlog_ring_emi_to_cache
* DESCRIPTION
*
* PARAMETERS
*
* RETURNS
*  void
*****************************************************************************/
static void connlog_ring_emi_to_cache(struct connlog_dev* handler)
{
	struct ring_emi_segment ring_emi_seg;
	struct ring_emi *ring_emi = &handler->log_buffer.ring_emi;
	struct ring *ring_cache = &handler->log_buffer.ring_cache;
	int total_size = 0;
	int count = 0;
	unsigned int cache_max_size = 0;
#ifndef DEBUG_LOG_ON
	static DEFINE_RATELIMIT_STATE(_rs, 10 * HZ, 1);
	static DEFINE_RATELIMIT_STATE(_rs2, HZ, 1);
#endif

	if (RING_FULL(ring_cache)) {
	#ifndef DEBUG_LOG_ON
		if (__ratelimit(&_rs))
	#endif
			pr_warn("[connlog] %s cache is full.\n", type_to_title[handler->conn_type]);
		return;
	}

	cache_max_size = RING_WRITE_REMAIN_SIZE(ring_cache);
	if (RING_EMI_EMPTY(ring_emi) || !ring_emi_read_prepare(cache_max_size, &ring_emi_seg, ring_emi)) {
	#ifndef DEBUG_LOG_ON
		if(__ratelimit(&_rs))
	#endif
			pr_err("[connlog] %s no data.\n", type_to_title[handler->conn_type]);
		return;
	}

	/* Check ring_emi buffer memory. Dump EMI data if it is corruption. */
	if (connlog_ring_emi_check(handler) == false) {
		pr_err("[connlog] %s emi check fail\n", type_to_title[handler->conn_type]);
		/* TODO: trigger assert by callback? */
		return;
	}

	RING_EMI_READ_ALL_FOR_EACH(ring_emi_seg, ring_emi) {
		struct ring_segment ring_cache_seg;
		unsigned int emi_buf_size = ring_emi_seg.sz;
		unsigned int written = 0;

#ifdef DEBUG_RING
		ring_emi_dump(__func__, ring_emi);
		ring_emi_dump_segment(__func__, &ring_emi_seg);
#endif
		RING_WRITE_FOR_EACH(ring_emi_seg.sz, ring_cache_seg, &handler->log_buffer.ring_cache) {
#ifdef DEBUG_RING
			ring_dump(__func__, &handler->log_buffer.ring_cache);
			ring_dump_segment(__func__, &ring_cache_seg);
#endif
		#ifndef DEBUG_LOG_ON
			if (__ratelimit(&_rs2))
		#endif
				pr_info("%s: ring_emi_seg.sz=%d, ring_cache_pt=%p, ring_cache_seg.sz=%d\n",
					type_to_title[handler->conn_type], ring_emi_seg.sz, ring_cache_seg.ring_pt,
					ring_cache_seg.sz);
			memcpy_fromio(ring_cache_seg.ring_pt, ring_emi_seg.ring_emi_pt + ring_cache_seg.data_pos,
				ring_cache_seg.sz);
			emi_buf_size -= ring_cache_seg.sz;
			written += ring_cache_seg.sz;
		}

		total_size += ring_emi_seg.sz;
		count++;
	}
}


/*****************************************************************************
 * FUNCTION
 *  connlog_fw_log_parser
 * DESCRIPTION
 *  Parse fw log and print to kernel
 * PARAMETERS
 *  conn_type      [IN]        log type
 *  buf            [IN]        buffer to prase
 *  sz             [IN]        buffer size
 * RETURNS
 *  void
 *****************************************************************************/
static void connlog_fw_log_parser(struct connlog_dev* handler, ssize_t sz)
{
	unsigned int systime = 0;
	unsigned int utc_s = 0;
	unsigned int utc_us = 0;
	unsigned int buf_len = 0;
	unsigned int print_len = 0;
	char* log_line = handler->log_line;
	const char* buf = handler->log_data;
	int conn_type = handler->conn_type;

	while (sz > LOG_HEAD_LENG) {
		if (*buf == log_head[0]) {
			if (!memcmp(buf, log_head, sizeof(log_head))) {
				buf_len = buf[14] + (buf[15] << 8);
				print_len = buf_len >= LOG_MAX_LEN ? LOG_MAX_LEN - 1 : buf_len;
				memcpy(log_line, buf + LOG_HEAD_LENG, print_len);
				log_line[print_len] = 0;
				pr_info("%s: %s\n", type_to_title[conn_type], log_line);
				sz -= (LOG_HEAD_LENG + buf_len);
				buf += (LOG_HEAD_LENG + buf_len);
				continue;
			} else if (sz >= TIMESYNC_LENG &&
				!memcmp(buf, timesync_head, sizeof(timesync_head))) {
				memcpy(&systime, buf + 28, sizeof(systime));
				memcpy(&utc_s, buf + 32, sizeof(utc_s));
				memcpy(&utc_us, buf + 36, sizeof(utc_us));
				pr_info("%s: timesync :  (%u) %u.%06u\n",
					type_to_title[conn_type], systime, utc_s, utc_us);
				sz -= TIMESYNC_LENG;
				buf += TIMESYNC_LENG;
				continue;
			}
		}
		sz--;
		buf++;
	}
}

/*****************************************************************************
 * FUNCTION
 *  connlog_ring_print
 * DESCRIPTION
 *  print log data on kernel log
 * PARAMETERS
 *  handler      [IN]        log handler
 * RETURNS
 *  void
 *****************************************************************************/
static void connlog_ring_print(struct connlog_dev* handler)
{
	unsigned int written = 0;
	unsigned int buf_size;
	struct ring_emi_segment ring_emi_seg;
	struct ring_emi *ring_emi = &handler->log_buffer.ring_emi;
	int conn_type = handler->conn_type;

	if (RING_EMI_EMPTY(ring_emi) || !ring_emi_read_all_prepare(&ring_emi_seg, ring_emi)) {
		pr_err("type(%s) no data, possibly taken by concurrent reader.\n", type_to_title[conn_type]);
		return;
	}
	buf_size = ring_emi_seg.remain;
	memset(handler->log_data, 0, handler->emi_size);

	/* Check ring_emi buffer memory. Dump EMI data if it is corruption. */
	if (connlog_ring_emi_check(handler) == false) {
		pr_err("[connlog] %s emi check fail\n", type_to_title[handler->conn_type]);
		/* TODO: trigger assert by callback? */
		return;
	}

	RING_EMI_READ_ALL_FOR_EACH(ring_emi_seg, ring_emi) {
		memcpy_fromio(handler->log_data + written, ring_emi_seg.ring_emi_pt, ring_emi_seg.sz);
		buf_size -= ring_emi_seg.sz;
		written += ring_emi_seg.sz;
	}

	if (conn_type != CONN_DEBUG_TYPE_BT)
		connlog_fw_log_parser(handler, written);
}

/*****************************************************************************
* FUNCTION
*  connlog_log_data_handler
* DESCRIPTION
*
* PARAMETERS
*
* RETURNS
*  void
*****************************************************************************/
static void connlog_log_data_handler(struct work_struct *work)
{
	struct connlog_dev* handler =
		container_of(work, struct connlog_dev, logDataWorker);
#ifndef DEBUG_LOG_ON
	static DEFINE_RATELIMIT_STATE(_rs, 10 * HZ, 1);
	static DEFINE_RATELIMIT_STATE(_rs2, 2 * HZ, 1);
#endif

	if (!RING_EMI_EMPTY(&handler->log_buffer.ring_emi)) {
		if (atomic_read(&g_log_mode) == LOG_TO_FILE)
			connlog_ring_emi_to_cache(handler);
		else
			connlog_ring_print(handler);
		connlog_event_set(handler);
	} else {
#ifndef DEBUG_LOG_ON
		if (__ratelimit(&_rs))
#endif
			pr_info("[connlog] %s emi ring is empty!\n",
				type_to_title[handler->conn_type]);
	}

#ifndef DEBUG_LOG_ON
	if (__ratelimit(&_rs2))
#endif
		pr_info("[connlog] %s irq counter = %d\n",
			type_to_title[handler->conn_type],
			EMI_READ32(handler->virAddrEmiLogBase + CONNLOG_IRQ_COUNTER_BASE));

	spin_lock_irqsave(&handler->irq_lock, handler->flags);
	if (handler->eirqOn)
		mod_timer(&handler->workTimer, jiffies + 1);
	spin_unlock_irqrestore(&handler->irq_lock, handler->flags);
}

static void connlog_do_schedule_work(struct connlog_dev* handler, bool count)
{
	spin_lock_irqsave(&handler->irq_lock, handler->flags);
	if (count) {
		handler->irq_counter++;
		EMI_WRITE32(
			handler->virAddrEmiLogBase + CONNLOG_IRQ_COUNTER_BASE,
			handler->irq_counter);
	}
	handler->eirqOn = !schedule_work(&handler->logDataWorker);
	spin_unlock_irqrestore(&handler->irq_lock, handler->flags);
}

/*****************************************************************************
* FUNCTION
*  connsys_log_get_buf_size
* DESCRIPTION
*  Get ring buffer unread size on EMI.
* PARAMETERS
*  conn_type      [IN]        subsys type
* RETURNS
*  unsigned int    Ring buffer unread size
*****************************************************************************/
unsigned int connsys_log_get_buf_size(int conn_type)
{
	struct connlog_dev* handler;
	if (conn_type < CONN_DEBUG_TYPE_WIFI || conn_type >= CONN_DEBUG_TYPE_END)
		return 0;

	handler = gLogDev[conn_type];
	if (handler == NULL) {
		pr_err("[%s][%s] didn't init\n", __func__, type_to_title[conn_type]);
		return 0;
	}

	return RING_SIZE(&handler->log_buffer.ring_cache);
}
EXPORT_SYMBOL(connsys_log_get_buf_size);

/*****************************************************************************
 * FUNCTION
 *  connsys_log_read_internal
 * DESCRIPTION
 *  Read log in ring_cache to buf
 * PARAMETERS
 *
 * RETURNS
 *
 *****************************************************************************/
static ssize_t connlog_read_internal(
	struct connlog_dev* handler, int conn_type,
	char *buf, char __user *userbuf, size_t count, bool to_user)
{
	unsigned int written = 0;
	unsigned int cache_buf_size;
	struct ring_segment ring_seg;
	struct ring *ring = &handler->log_buffer.ring_cache;
	unsigned int size = 0;
	int retval;
	static DEFINE_RATELIMIT_STATE(_rs, 10 * HZ, 1);
	static DEFINE_RATELIMIT_STATE(_rs2, 1 * HZ, 1);

	size = count < RING_SIZE(ring) ? count : RING_SIZE(ring);
	if (RING_EMPTY(ring) || !ring_read_prepare(size, &ring_seg, ring)) {
		pr_err("type(%d) no data, possibly taken by concurrent reader.\n", conn_type);
		goto done;
	}
	cache_buf_size = ring_seg.remain;

	RING_READ_FOR_EACH(size, ring_seg, ring) {
		if (to_user) {
			retval = copy_to_user(userbuf + written, ring_seg.ring_pt, ring_seg.sz);
			if (retval) {
				if (__ratelimit(&_rs))
					pr_err("copy to user buffer failed, ret:%d\n", retval);
				goto done;
			}
		} else {
			memcpy(buf + written, ring_seg.ring_pt, ring_seg.sz);
		}
		cache_buf_size -= ring_seg.sz;
		written += ring_seg.sz;
		if (__ratelimit(&_rs2))
			pr_info("[%s] copy %d to %s\n",
				type_to_title[conn_type],
				ring_seg.sz,
				(to_user? "user space" : "buffer"));
	}
done:
	return written;
}

/*****************************************************************************
 * FUNCTION
 *  connsys_log_read_to_user
 * DESCRIPTION
 *  Read log in ring_cache to user space buf
 * PARAMETERS
 *
 * RETURNS
 *
 *****************************************************************************/
ssize_t connsys_log_read_to_user(int conn_type, char __user *buf, size_t count)
{
	struct connlog_dev* handler;
	unsigned int written = 0;

	if (conn_type < CONN_DEBUG_TYPE_WIFI || conn_type >= CONN_DEBUG_TYPE_END)
		goto done;
	if (atomic_read(&g_log_mode) != LOG_TO_FILE)
		goto done;

	handler = gLogDev[conn_type];
	if (handler == NULL) {
		pr_err("[%s][%s] not init\n", __func__, type_to_title[conn_type]);
		goto done;
	}
	written = connlog_read_internal(handler, conn_type, NULL, buf, count, true);
done:
	return written;
}
EXPORT_SYMBOL(connsys_log_read_to_user);

/*****************************************************************************
 * FUNCTION
 *  connsys_log_read
 * DESCRIPTION
 *  Read log in ring_cache to buf
 * PARAMETERS
 *
 * RETURNS
 *
 *****************************************************************************/
ssize_t connsys_log_read(int conn_type, char *buf, size_t count)
{
	unsigned int ret = 0;
	struct connlog_dev* handler;

	if (conn_type < CONN_DEBUG_TYPE_WIFI || conn_type >= CONN_DEBUG_TYPE_END)
		goto done;
	if (atomic_read(&g_log_mode) != LOG_TO_FILE)
		goto done;

	handler = gLogDev[conn_type];
	ret = connlog_read_internal(handler, conn_type, buf, NULL, count, false);
done:
	return ret;
}
EXPORT_SYMBOL(connsys_log_read);


/*****************************************************************************
 * FUNCTION
 *  connsys_dedicated_log_set_log_mode
 * DESCRIPTION
 *  set log mode.
 * PARAMETERS
 *  mode            [IN]        log mode
 * RETURNS
 *  void
 *****************************************************************************/
void connsys_dedicated_log_set_log_mode(int mode)
{
	atomic_set(&g_log_mode, (mode > 0 ? LOG_TO_FILE : PRINT_TO_KERNEL_LOG));
}
EXPORT_SYMBOL(connsys_dedicated_log_set_log_mode);

/*****************************************************************************
* FUNCTION
*  connsys_dedicated_log_get_log_mode
* DESCRIPTION
*  get log mode.
* PARAMETERS
*  void
* RETURNS
* int    log mode
*****************************************************************************/
int connsys_dedicated_log_get_log_mode(void)
{
	return atomic_read(&g_log_mode);
}
EXPORT_SYMBOL(connsys_dedicated_log_get_log_mode);

/*****************************************************************************
* FUNCTION
*  connsys_log_irq_handler
* DESCRIPTION
*
* PARAMETERS
*  void
* RETURNS
*  int
*****************************************************************************/
int connsys_log_irq_handler(int conn_type)
{
	struct connlog_dev* handler;
	if (conn_type < CONN_DEBUG_TYPE_WIFI || conn_type >= CONN_DEBUG_TYPE_END)
		return -1;

	handler = gLogDev[conn_type];
	if (handler == NULL) {
		pr_err("[%s][%s] didn't init\n", __func__, type_to_title[conn_type]);
		return -1;
	}

	connlog_do_schedule_work(handler, true);
	return 0;
}
EXPORT_SYMBOL(connsys_log_irq_handler);

/*****************************************************************************
* FUNCTION
*  connsys_log_register_event_cb
* DESCRIPTION
*·
* PARAMETERS
*  void
* RETURNS
* 
*****************************************************************************/
int connsys_log_register_event_cb(int conn_type, CONNLOG_EVENT_CB func)
{
	struct connlog_dev* handler;
	if (conn_type < CONN_DEBUG_TYPE_WIFI || conn_type >= CONN_DEBUG_TYPE_END)
		return -1;

	handler = gLogDev[conn_type];
	if (handler == NULL) {
		pr_err("[%s][%s] didn't init\n", __func__, type_to_title[conn_type]);
		return -1;
	}

	handler->callback.log_data_handler = func;
	return 0;
}
EXPORT_SYMBOL(connsys_log_register_event_cb);

/*****************************************************************************
* FUNCTION
*  connlog_subsys_init
* DESCRIPTION
* 
* PARAMETERS
*  conn_type	[IN]	subsys type
*  emi_addr	[IN]	physical emi
*  emi_size	[IN]	emi size
* RETURNS
*  struct connlog_dev* the handler 
*****************************************************************************/
static struct connlog_dev* connlog_subsys_init(
	int conn_type,
	phys_addr_t emi_addr,
	unsigned int emi_size)
{
	struct connlog_dev* handler = 0;

	if (conn_type < CONN_DEBUG_TYPE_WIFI || conn_type >= CONN_DEBUG_TYPE_END)
		return 0;

	handler = (struct connlog_dev*)kzalloc(sizeof(struct connlog_dev), GFP_KERNEL);
	if (!handler)
		return 0;

	handler->conn_type = conn_type;
	if (connlog_emi_init(handler, emi_addr, emi_size)) {
		pr_err("[%s] EMI init failed\n", type_to_title[conn_type]);
		goto error_exit;
	}

	if (connlog_ring_buffer_init(handler)) {
		pr_err("[%s] Ring buffer init failed\n", type_to_title[conn_type]);
		goto error_exit;
	}

	init_timer(&handler->workTimer);
	handler->workTimer.data = (unsigned long)handler;
	handler->workTimer.function = work_timer_handler;
	handler->irq_counter = 0;
	spin_lock_init(&handler->irq_lock);
	INIT_WORK(&handler->logDataWorker, connlog_log_data_handler);

	/* alarm timer */
	return handler;

error_exit:
	if (handler)
		connlog_subsys_deinit(handler);
	return 0;

}

/*****************************************************************************
* FUNCTION
*  connsys_log_init
* DESCRIPTION
* 
* PARAMETERS
*  void
* RETURNS
*  int
*****************************************************************************/
int connsys_log_init(int conn_type)
{
	struct connlog_dev* handler;
	phys_addr_t log_start_addr;
	unsigned int log_size;
	struct connlog_emi_config* emi_config;

	if (conn_type < CONN_DEBUG_TYPE_WIFI || conn_type >= CONN_DEBUG_TYPE_END) {
		pr_err("[%s] invalid type:%d\n", __func__, conn_type);
		return -1;
	}
	if (gLogDev[conn_type] != NULL) {
		pr_err("[%s][%s] double init.\n", __func__, type_to_title[conn_type]);
		return 0;
	}

	emi_config = get_connsyslog_platform_config(conn_type);
	if (!emi_config) {
		pr_err("[%s] get emi config fail.\n", __func__);
		return -1;
	}

	log_start_addr = emi_config->log_offset + gPhyEmiBase;
	log_size = emi_config->log_size;
	pr_info("%s init. Base=%p size=%d\n",
		type_to_title[conn_type], log_start_addr, log_size);

	handler = connlog_subsys_init(conn_type, log_start_addr, log_size);
	if (handler == NULL) {
		pr_err("[%s][%s] failed.\n", __func__, type_to_title[conn_type]);
		return -1;
	}

	gLogDev[conn_type] = handler;
	return 0;
}
EXPORT_SYMBOL(connsys_log_init);

/*****************************************************************************
* Function
*  connlog_subsys_deinit
* DESCRIPTION
*
* PARAMETERS
*
* RETURNS
*
*****************************************************************************/
static void connlog_subsys_deinit(struct connlog_dev* handler)
{
	if (handler == NULL)
		return;

	connlog_emi_deinit(handler);
	connlog_ring_buffer_deinit(handler);
	kfree(handler);
}

/*****************************************************************************
* Function
*  connsys_log_deinit
* DESCRIPTION
*
* PARAMETERS
*
* RETURNS
*
*****************************************************************************/
int connsys_log_deinit(int conn_type)
{
	struct connlog_dev* handler;
	if (conn_type < CONN_DEBUG_TYPE_WIFI || conn_type >= CONN_DEBUG_TYPE_END)
		return -1;

	handler = gLogDev[conn_type];
	if (handler == NULL) {
		pr_err("[%s][%s] didn't init\n", __func__, type_to_title[conn_type]);
		return -1;
	}

	connlog_subsys_deinit(gLogDev[conn_type]);
	gLogDev[conn_type] = NULL;
	return 0;
}
EXPORT_SYMBOL(connsys_log_deinit);

/*****************************************************************************
* FUNCTION
*  connsys_log_get_utc_time
* DESCRIPTION
*  Return UTC time
* PARAMETERS
*  second         [IN]        UTC seconds
*  usecond        [IN]        UTC usecons
* RETURNS
*  void
*****************************************************************************/
void connsys_log_get_utc_time(
	unsigned int *second, unsigned int *usecond)
{
	struct timeval time;

	do_gettimeofday(&time);
	*second = (unsigned int)time.tv_sec; /* UTC time second unit */
	*usecond = (unsigned int)time.tv_usec; /* UTC time microsecond unit */
}
EXPORT_SYMBOL(connsys_log_get_utc_time);

static inline bool connlog_is_alarm_enable(void)
{
	if ((gLogAlarm.alarm_state & CONNLOG_ALARM_STATE_ENABLE) > 0)
		return true;
	return false;
}

static int connlog_set_alarm_timer(void)
{
	ktime_t kt;

	kt = ktime_set(gLogAlarm.alarm_sec, 0);
	alarm_start_relative(&gLogAlarm.alarm_timer, kt);

	pr_info("[connsys_log_alarm] alarm timer enabled timeout=[%d]", gLogAlarm.alarm_sec);
	return 0;
}

static int connlog_cancel_alarm_timer(void)
{
	pr_info("[connsys_log_alarm] alarm timer cancel");
	return alarm_cancel(&gLogAlarm.alarm_timer);
}


static enum alarmtimer_restart connlog_alarm_timer_handler(struct alarm *alarm,
	ktime_t now)
{
	ktime_t kt;
	struct rtc_time tm;
	unsigned int tsec, tusec;
	int i;

	connsys_log_get_utc_time(&tsec, &tusec);
	rtc_time_to_tm(tsec, &tm);
	pr_info("[connsys_log_alarm] alarm_timer triggered [%d-%02d-%02d %02d:%02d:%02d.%09u]"
			, tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday
			, tm.tm_hour, tm.tm_min, tm.tm_sec, tusec);

	for (i = 0; i < CONN_DEBUG_TYPE_END; i++) {
		if (gLogDev[i]) {
			connlog_do_schedule_work(gLogDev[i], false);
		}
	}

	spin_lock_irqsave(&gLogAlarm.alarm_lock, gLogAlarm.flags);
	kt = ktime_set(gLogAlarm.alarm_sec, 0);
	alarm_start_relative(&gLogAlarm.alarm_timer, kt);
	spin_unlock_irqrestore(&gLogAlarm.alarm_lock, gLogAlarm.flags);

	return ALARMTIMER_NORESTART;
}

static int connlog_alarm_init(void)
{
	alarm_init(&gLogAlarm.alarm_timer, ALARM_REALTIME, connlog_alarm_timer_handler);
	gLogAlarm.alarm_state = CONNLOG_ALARM_STATE_DISABLE;
	spin_lock_init(&gLogAlarm.alarm_lock);

	return 0;
}

/*****************************************************************************
* FUNCTION
*  connsys_dedicated_log_path_alarm_enable
* DESCRIPTION
*  Enable log timer.
*  When log timer is enable, it starts every sec seconds to fetch log from EMI
*  to file.
*  Usually enable log timer for debug.
* PARAMETERS
*  sec         [IN]       timer config 
* RETURNS
*  int
*****************************************************************************/
int connsys_dedicated_log_path_alarm_enable(unsigned int sec)
{
	if (!gPhyEmiBase)
		return -1;

	spin_lock_irqsave(&gLogAlarm.alarm_lock, gLogAlarm.flags);

	gLogAlarm.alarm_sec = sec;
	if (!connlog_is_alarm_enable()) {
		gLogAlarm.alarm_state = CONNLOG_ALARM_STATE_ENABLE;
		pr_info("[connsys_log_alarm] alarm timer enabled timeout=[%d]", sec);
	}
	if (gLogAlarm.blank_state == 0)
		connlog_set_alarm_timer();

	spin_unlock_irqrestore(&gLogAlarm.alarm_lock, gLogAlarm.flags);
	return 0;
}
EXPORT_SYMBOL(connsys_dedicated_log_path_alarm_enable);

/*****************************************************************************
* FUNCTION
*  connsys_dedicated_log_path_alarm_disable
* DESCRIPTION
*  Disable log timer
* PARAMETERS
*
* RETURNS
*  int
*****************************************************************************/
int connsys_dedicated_log_path_alarm_disable(void)
{
	int ret;

	if (!gPhyEmiBase)
		return -1;

	spin_lock_irqsave(&gLogAlarm.alarm_lock, gLogAlarm.flags);

	if (connlog_is_alarm_enable()) {
		ret = connlog_cancel_alarm_timer();
		gLogAlarm.alarm_state = CONNLOG_ALARM_STATE_ENABLE;
		pr_info("[connsys_log_alarm] alarm timer disable ret=%d", ret);
	}
	spin_unlock_irqrestore(&gLogAlarm.alarm_lock, gLogAlarm.flags);
	return 0;
}
EXPORT_SYMBOL(connsys_dedicated_log_path_alarm_disable);

/****************************************************************************
* FUNCTION
*  connsys_dedicated_log_path_blank_state_changed
* DESCRIPTION
* 
* PARAMETERS
*
* RETURNS
*  int
*****************************************************************************/
int connsys_dedicated_log_path_blank_state_changed(int blank_state)
{
	int ret = 0;

	if (!gPhyEmiBase)
		return -1;
	spin_lock_irqsave(&gLogAlarm.alarm_lock, gLogAlarm.flags);
	gLogAlarm.blank_state = blank_state;
	if (connlog_is_alarm_enable()) {
		if (blank_state == 0)
			ret = connlog_set_alarm_timer();
		else
			ret = connlog_cancel_alarm_timer();
	}

	spin_unlock_irqrestore(&gLogAlarm.alarm_lock, gLogAlarm.flags);

	return ret;
}
EXPORT_SYMBOL(connsys_dedicated_log_path_blank_state_changed);

/*****************************************************************************
* FUNCTION
*  connsys_dedicated_log_path_apsoc_init
* DESCRIPTION
*  Initialize API for common driver to initialize connsys dedicated log
*  for APSOC platform
* PARAMETERS
*  emiaddr      [IN]        EMI physical base address
* RETURNS
*  void
****************************************************************************/
int connsys_dedicated_log_path_apsoc_init(phys_addr_t emiaddr)
{
	if (gPhyEmiBase != 0 || emiaddr == 0) {
		pr_err("Connsys log double init or invalid parameter(emiaddr=%p)\n", emiaddr);
		return -1;
	}

	gPhyEmiBase = emiaddr;

	connlog_alarm_init();
	return 0;
}
EXPORT_SYMBOL(connsys_dedicated_log_path_apsoc_init);

/*****************************************************************************
* FUNCTION
*  connsys_dedicated_log_path_apsoc_deinit
* DESCRIPTION
*  De-Initialize API for common driver to release cache, un-remap emi and free
*  irq for APSOC platform
* PARAMETERS
*  void
* RETURNS
*  void
*****************************************************************************/
int connsys_dedicated_log_path_apsoc_deinit(void)
{
	int i;

	/* Check subsys */
	for (i = 0; i < CONN_DEBUG_TYPE_END; i++) {
		if (gLogDev[i] != NULL) {
			pr_err("[%s] subsys %s should be deinit first.\n",
				__func__, type_to_title[i]);
			return -1;
		}
	}

	gPhyEmiBase = 0;
	return 0;
}
EXPORT_SYMBOL(connsys_dedicated_log_path_apsoc_deinit);
