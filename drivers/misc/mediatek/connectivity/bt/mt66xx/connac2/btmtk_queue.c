/*  SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

 #include "btmtk_btif.h"
 #include <linux/rtc.h>

 /*******************************************************************************
 *			       D A T A	 T Y P E S
 ********************************************************************************
 */

 /*******************************************************************************
 *			      P U B L I C   D A T A
 ********************************************************************************
 */
struct workqueue_struct *workqueue_task;
struct delayed_work work;

 /*******************************************************************************
 *			     P R I V A T E   D A T A
 ********************************************************************************
 */
extern struct btmtk_dev *g_bdev;
extern struct bt_dbg_st g_bt_dbg_st;

/*******************************************************************************
*                  F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/
#if (USE_DEVICE_NODE == 1)
uint8_t is_rx_queue_empty(void)
{
	struct bt_ring_buffer_mgmt *p_ring = &g_bdev->rx_buffer;

	spin_lock(&p_ring->lock);
	if (p_ring->read_idx == p_ring->write_idx) {
		spin_unlock(&p_ring->lock);
		return TRUE;
	} else {
		spin_unlock(&p_ring->lock);
		return FALSE;
	}
}

static uint8_t is_rx_queue_res_available(uint32_t length)
{
	uint32_t room_left;
	struct bt_ring_buffer_mgmt *p_ring = &g_bdev->rx_buffer;

	/*
	 * Get available space of RX Queue
	 */
	spin_lock(&p_ring->lock);
	if (p_ring->read_idx <= p_ring->write_idx)
		room_left = RING_BUFFER_SIZE - p_ring->write_idx + p_ring->read_idx - 1;
	else
		room_left = p_ring->read_idx - p_ring->write_idx - 1;
	spin_unlock(&p_ring->lock);

	if (room_left < length) {
		BTMTK_WARN("RX queue room left (%u) < required (%u)", room_left, length);
		return FALSE;
	}
	return TRUE;
}

static int32_t rx_pkt_enqueue(uint8_t *buffer, uint32_t length)
{
	uint32_t tail_len;
	struct bt_ring_buffer_mgmt *p_ring = &g_bdev->rx_buffer;

	if (length > HCI_MAX_FRAME_SIZE) {
		BTMTK_ERR("Abnormal packet length %u, not enqueue!", length);
		return -EINVAL;
	}

	spin_lock(&p_ring->lock);
	if (p_ring->write_idx + length < RING_BUFFER_SIZE) {
		memcpy(p_ring->buf + p_ring->write_idx, buffer, length);
		p_ring->write_idx += length;
	} else {
		tail_len = RING_BUFFER_SIZE - p_ring->write_idx;
		memcpy(p_ring->buf + p_ring->write_idx, buffer, tail_len);
		memcpy(p_ring->buf, buffer + tail_len, length - tail_len);
		p_ring->write_idx = length - tail_len;
	}
	spin_unlock(&p_ring->lock);

	return 0;

}

int32_t rx_skb_enqueue(struct sk_buff *skb)
{
	#define WAIT_TIMES 40
	int8_t i = 0;
	int32_t ret = 0;

	if (skb->len == 0) {
		BTMTK_WARN("Inavlid data event, skip, length = %d", skb->len);
		return -1;
	}

	/* FW will block the data if it's buffer is full,
	   driver can wait a interval for native process to read out */
	if(g_bt_dbg_st.rx_buf_ctrl == TRUE) {
		for(i = 0; i < WAIT_TIMES; i++) {
			if (!is_rx_queue_res_available(skb->len + 1)) {
				usleep_range(USLEEP_5MS_L, USLEEP_5MS_H);
			} else
				break;
		}
	}
	if (!is_rx_queue_res_available(skb->len + 1)) {
		BTMTK_WARN("rx packet drop!!!");
		return -1;
	}

	memcpy(skb_push(skb, 1), &bt_cb(skb)->pkt_type, 1);
	ret = rx_pkt_enqueue(skb->data, skb->len);

	if (!is_rx_queue_empty())
		g_bdev->rx_event_cb();

	return ret;
}

void rx_dequeue(uint8_t *buffer, uint32_t size, uint32_t *plen)
{
	uint32_t copy_len = 0, tail_len;
	struct bt_ring_buffer_mgmt *p_ring = &g_bdev->rx_buffer;

	spin_lock(&p_ring->lock);
	if (p_ring->read_idx != p_ring->write_idx) {
		/*
		 * RX Queue not empty,
		 * fill out the retrieving buffer untill it is full, or we have no data.
		 */
		if (p_ring->read_idx < p_ring->write_idx) {
			copy_len = p_ring->write_idx - p_ring->read_idx;
			if (copy_len > size)
				copy_len = size;
			memcpy(buffer, p_ring->buf + p_ring->read_idx, copy_len);
			p_ring->read_idx += copy_len;
		} else { /* read_idx > write_idx */
			tail_len = RING_BUFFER_SIZE - p_ring->read_idx;
			if (tail_len > size) { /* exclude equal case to skip wrap check */
				copy_len = size;
				memcpy(buffer, p_ring->buf + p_ring->read_idx, copy_len);
				p_ring->read_idx += copy_len;
			} else {
				/* 1. copy tail */
				memcpy(buffer, p_ring->buf + p_ring->read_idx, tail_len);
				/* 2. check if head length is enough */
				copy_len = (p_ring->write_idx < (size - tail_len))
					   ? p_ring->write_idx : (size - tail_len);
				/* 3. copy header */
				memcpy(buffer + tail_len, p_ring->buf, copy_len);
				p_ring->read_idx = copy_len;
				/* 4. update copy length: head + tail */
				copy_len += tail_len;
			}
		}
	}
	spin_unlock(&p_ring->lock);

	*plen = copy_len;
	return;
}

void rx_queue_flush(void)
{
	struct bt_ring_buffer_mgmt *p_ring = &g_bdev->rx_buffer;
	p_ring->read_idx = p_ring->write_idx = 0;
}

void rx_queue_initialize(void)
{
	struct bt_ring_buffer_mgmt *p_ring = &g_bdev->rx_buffer;

	p_ring->read_idx = p_ring->write_idx = 0;
	spin_lock_init(&p_ring->lock);
}

void rx_queue_destroy(void)
{
	rx_queue_flush();
}
#endif // (USE_DEVICE_NODE == 1)

#if (DRIVER_CMD_CHECK == 1)

void cmd_list_initialize(void)
{
	struct bt_cmd_queue *p_queue = &g_bdev->cmd_queue;
	BTMTK_DBG("%s", __func__);

	p_queue->head = NULL;
	p_queue->tail = NULL;
	p_queue->size = 0;
	spin_lock_init(&p_queue->lock);
}

struct bt_cmd_node* cmd_free_node(struct bt_cmd_node* node)
{
	struct bt_cmd_queue *p_queue = &g_bdev->cmd_queue;

	struct bt_cmd_node* next = node->next;
	kfree(node);
	p_queue->size--;

	return next;
}

bool cmd_list_isempty(void)
{
	struct bt_cmd_queue *p_queue = &g_bdev->cmd_queue;

	spin_lock(&p_queue->lock);
	if(p_queue->size == 0) {
		spin_unlock(&p_queue->lock);
		return TRUE;
	} else {
		spin_unlock(&p_queue->lock);
		return FALSE;
	}
}

bool cmd_list_append (uint16_t opcode)
{
	struct bt_cmd_queue *p_queue = &g_bdev->cmd_queue;
	struct bt_cmd_node *node = kzalloc(sizeof(struct bt_cmd_node),GFP_KERNEL);

	if (!node) {
		BTMTK_ERR("%s create node fail",__func__);
		return FALSE;
	}
	spin_lock(&p_queue->lock);
	node->next = NULL;
	node->opcode = opcode;

	if(p_queue->tail == NULL){
		p_queue->head = node;
		p_queue->tail = node;
	} else {
		p_queue->tail->next = node;
		p_queue->tail = node;
	}
	p_queue->size ++;

	spin_unlock(&p_queue->lock);

	return TRUE;
}

bool cmd_list_check(uint16_t opcode)
{
	struct bt_cmd_queue *p_queue = &g_bdev->cmd_queue;
	struct bt_cmd_node* curr = NULL;

	if (cmd_list_isempty() == TRUE) return FALSE;
	spin_lock(&p_queue->lock);

	curr = p_queue->head;

	while(curr){
		if(curr->opcode == opcode){
			spin_unlock(&p_queue->lock);
			return TRUE;
		}
		curr=curr->next;
	}
	spin_unlock(&p_queue->lock);

	return FALSE;
}

bool cmd_list_remove(uint16_t opcode)
{
	struct bt_cmd_queue *p_queue = &g_bdev->cmd_queue;
	struct bt_cmd_node* prev = NULL;
	struct bt_cmd_node* curr = NULL;

	if (cmd_list_isempty() == TRUE) return FALSE;

	spin_lock(&p_queue->lock);

	if(p_queue->head->opcode == opcode) {
		struct bt_cmd_node* next = cmd_free_node(p_queue->head);
		if (p_queue->head == p_queue->tail) p_queue->tail = NULL;
		p_queue->head = next;
		spin_unlock(&p_queue->lock);
		return TRUE;
	}

	prev = p_queue->head;
	curr = p_queue->head->next;

	while(curr){
		if(curr->opcode == opcode) {
			prev->next = cmd_free_node(curr);
			if(p_queue->tail == curr) p_queue->tail = prev;
			spin_unlock(&p_queue->lock);
			return TRUE;
		}
		prev = curr;
		curr = curr->next;
	}
	BTMTK_ERR("%s No match opcode: %4X", __func__,opcode);
	spin_unlock(&p_queue->lock);
	return FALSE;
}

void cmd_list_destory(void)
{
	struct bt_cmd_queue *p_queue = &g_bdev->cmd_queue;
	struct bt_cmd_node* curr = p_queue->head;
	BTMTK_DBG("%s",__func__);

	while(curr){
		curr = cmd_free_node(curr);
	}
	p_queue->head = NULL;
	p_queue->tail = NULL;
	p_queue->size = 0;
}

void command_response_timeout(struct work_struct *pwork)
{
	struct bt_cmd_queue *p_queue = &g_bdev->cmd_queue;

	if (p_queue->size != 0) {
		g_bdev->cmd_timeout_count++;

		BTMTK_INFO("[%s] timeout [%d] sleep [%d] force_on [%d]", __func__,
							g_bdev->cmd_timeout_count,
							g_bdev->psm.sleep_flag,
							g_bdev->psm.force_on);
		btmtk_cif_dump_rxd_backtrace();
		btmtk_cif_dump_fw_no_rsp(BT_BTIF_DUMP_REG);
		if (g_bdev->cmd_timeout_count == 4) {
			BTMTK_ERR("%s,  !!!! Command Timeout !!!!  opcode 0x%4X", __func__, p_queue->head->opcode);
			// To-do : Need to consider if it has any condition to check
			g_bdev->cmd_timeout_count = 0;
			bt_trigger_reset();
		} else
			queue_delayed_work(workqueue_task, &work, HZ>>1);
	}
}

bool cmd_workqueue_init(void)
{
	BTMTK_INFO("init workqueue");
	workqueue_task = create_singlethread_workqueue("workqueue_task");
	if(!workqueue_task){
		BTMTK_ERR("fail to init workqueue");
		return FALSE;
	}
	INIT_DELAYED_WORK(&work, command_response_timeout);
	g_bdev->cmd_timeout_count = 0;
	return TRUE;
}

void update_command_response_workqueue(void) {
	struct bt_cmd_queue *p_queue = &g_bdev->cmd_queue;
	if (p_queue->size == 0){
		BTMTK_DBG("command queue size = 0");
		cancel_delayed_work(&work);
	} else {
		BTMTK_DBG("update new command queue : %4X" , p_queue->head->opcode);
		g_bdev->cmd_timeout_count = 0;
		cancel_delayed_work(&work);
		queue_delayed_work(workqueue_task, &work, HZ>>1);
	}
}

void cmd_workqueue_exit(void)
{
	BTMTK_INFO("exit workqueue");
	if(workqueue_task == NULL)
		return;
	cancel_delayed_work(&work);
	flush_workqueue(workqueue_task);
	destroy_workqueue(workqueue_task);
	workqueue_task = NULL;
}

#endif // (DRIVER_CMD_CHECK == 1)


const char* direction_tostring (enum bt_direction_type direction_type) {
	char *type[] = {"NONE", "TX", "RX"};
	return type[direction_type];
}

void dump_queue_initialize(void)
{
	struct bt_dump_queue *d_queue = &g_bdev->dump_queue;
	BTMTK_INFO("init dumpqueue");

	d_queue->index = 0;
	d_queue->full = 0;
	spin_lock_init(&d_queue->lock);
	memset(d_queue->queue, 0, MAX_DUMP_QUEUE_SIZE * sizeof(struct bt_dump_packet));
}


void add_dump_packet(const uint8_t *buffer,const uint32_t length, enum bt_direction_type type){
	struct bt_dump_queue *d_queue = &g_bdev->dump_queue;
	uint32_t index = d_queue->index;
	struct bt_dump_packet *p_packet = &d_queue->queue[index];
	uint32_t copysize;

	spin_lock(&d_queue->lock);
	if (length > MAX_DUMP_DATA_SIZE)
		copysize = MAX_DUMP_DATA_SIZE;
	else
		copysize = length;

	do_gettimeofday(&p_packet->time);
	ktime_get_ts(&p_packet->kerneltime);
	memcpy(p_packet->data,buffer,copysize);
	p_packet->data_length = length;
	p_packet->direction_type = type;

	d_queue->index = (d_queue->index+1) % MAX_DUMP_QUEUE_SIZE;
	BTMTK_DBG("index: %d", d_queue->index);
	if (d_queue->full == FALSE && d_queue->index == 0)
		d_queue->full = TRUE;
	spin_unlock(&d_queue->lock);
}

void print_dump_packet(struct bt_dump_packet *p_packet){
	int32_t copysize;
	uint32_t sec, usec, ksec, knsec;
	struct rtc_time tm;

	sec = p_packet->time.tv_sec;
	usec = p_packet->time.tv_usec;

	ksec = p_packet->kerneltime.tv_sec;
	knsec = p_packet->kerneltime.tv_nsec;

	rtc_time_to_tm(sec, &tm);

	if (p_packet->data_length > MAX_DUMP_DATA_SIZE)
		copysize = MAX_DUMP_DATA_SIZE;
	else
		copysize = p_packet->data_length;

	BTMTK_INFO_RAW(p_packet->data, copysize, "Dump: Time:%02d:%02d:%02d.%06u, Kernel Time:%6d.%09u, %s, Size = %3d, Data: "
	, tm.tm_hour+8, tm.tm_min, tm.tm_sec, usec, ksec, knsec
	, direction_tostring(p_packet->direction_type), p_packet->data_length);
}

void show_all_dump_packet(void) {
	struct bt_dump_queue *d_queue = &g_bdev->dump_queue;
	int32_t i, j, showsize;
	struct bt_dump_packet *p_packet;

	spin_lock(&d_queue->lock);
	if (d_queue->full == TRUE) {
		showsize = MAX_DUMP_QUEUE_SIZE;
		for(i = 0,j = d_queue->index; i < showsize; i++,j++) {
			p_packet = &d_queue->queue[j % MAX_DUMP_QUEUE_SIZE];
			print_dump_packet(p_packet);
		}
	} else {
		showsize = d_queue->index;
		for(i = 0; i < showsize; i++) {
			p_packet = &d_queue->queue[i];
			print_dump_packet(p_packet);
		}
	}
	spin_unlock(&d_queue->lock);
}
