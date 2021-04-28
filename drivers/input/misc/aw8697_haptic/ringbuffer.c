/*
* reader shoule NEVER block !!!, if no data is avaliable, just return zero
* writer will block if there is no space, and wakend up by reader or force exit
* there is 1 writer and 1 reader for each buffer, so no lock is used
* writer shoule read rd_index to check if free size is enought, and  then fill this buffer  update wr_index
* reader shoule read wr_index to check if avaliable size is enought, and then read the buffer and update rd_index
* empty: wr_index==rd_index
* full: (wr_index +1) % BUFFER_SIZE == rd_index
* total avaliable size is BUFFER_SIZE -1
*/
#define DEBUG
#include <linux/errno.h>
#include "ringbuffer.h"

#define MIN(x, y) ((x) < (y) ? (x) : (y))

#define BUFFER_SIZE (1024 * 8 + 1)

struct rb {
	char *gbuffer;
	atomic_t wr_index;
	atomic_t rd_index;
	atomic_t eof;
	volatile  int32_t aval_size; // avalibale to write size.
	atomic_t buf_condition, exit;
	wait_queue_head_t wait_q;
};

struct rb *grb;

int32_t get_free_size(int32_t tail, int32_t head)
{
	if (head == tail)
		return BUFFER_SIZE - 1;
	else if (head < tail)
		return (head + BUFFER_SIZE - 1 - tail);
	else
		return(head - tail - 1);
}

int write_rb(const char *data, int32_t size)
{
	int32_t tail = atomic_read(&grb->wr_index);
	int32_t head = atomic_read(&grb->rd_index);
	int32_t part;
	int32_t ret;
	grb->aval_size = get_free_size(tail, head);

	pr_debug("write  write index %d, read index %d, free size %d", tail, head, grb->aval_size);

	while ((grb->aval_size < size) && (!atomic_read(&grb->exit))) {
		pr_debug("no space avaliable");
		pr_info("%s  goint to waiting irq exit\n", __func__);
		ret = wait_event_interruptible(grb->wait_q, atomic_read(&grb->buf_condition) == 1);
		if (ret == -ERESTARTSYS) {
			 pr_err("%s wake up by signal return erro\n", __func__);
			 return ret;
		}

		atomic_set(&grb->buf_condition, 0);
		tail = atomic_read(&grb->wr_index);
		head = atomic_read(&grb->rd_index);
		grb->aval_size = get_free_size(tail, head);
	}
	if (atomic_read(&grb->exit) == 1) {
		pr_debug("exit write_rb");
		return -EPERM;
	}

	part =  BUFFER_SIZE - tail;
	if (part < size) {
		memcpy(grb->gbuffer + tail, data, part);
		memcpy(grb->gbuffer, data + part, size - part);
		tail = size - part;
	} else {
		memcpy(grb->gbuffer + tail, data, size);
		tail += size;
		if (tail >= BUFFER_SIZE)
		tail = tail % BUFFER_SIZE;
	}
	atomic_set(&grb->wr_index, tail);
	grb->aval_size = get_free_size(tail, head);
	pr_debug("after write %d,  write index %d, read index %d, aval_size %d", size, tail, head, grb->aval_size);
	return size;
}

int read_rb(char *data, int32_t size)
{
	int32_t tail;
	int32_t head;
	int32_t filled_size;
	void *buf;
	int32_t read_bytes, part;
	buf = data;

	pr_debug("read_rb data:%p, size %d", data, (int)size);

	tail = atomic_read(&grb->wr_index);
	head = atomic_read(&grb->rd_index);
	grb->aval_size = get_free_size(tail, head);
	filled_size = BUFFER_SIZE - 1 - grb->aval_size; // aready write size.

	pr_debug("write index %d, read index %d, filled size %d", tail, head, filled_size);
	read_bytes = MIN (size, filled_size);
	if (size > filled_size)
		pr_debug("buffer underrun , req size %d, filled size %d", size, filled_size);
	part = BUFFER_SIZE - head;
	if (part < read_bytes) {
		memcpy(buf, grb->gbuffer + head, part);
		memcpy((char *)buf + part, grb->gbuffer, read_bytes - part);
		head = read_bytes - part;
	} else {
		memcpy(buf, grb->gbuffer + head, read_bytes);
		head += read_bytes;
		if (head >= BUFFER_SIZE)
			head = head % BUFFER_SIZE;
	}
	atomic_set(&grb->rd_index, head);
	grb->aval_size = get_free_size(tail, head);

	//add wakeup here
	atomic_set(&grb->buf_condition, 1);
	wake_up_interruptible(&grb->wait_q);
	pr_debug("read_rb: after read %d  write index %d, read index %d, aval_size %d", read_bytes, tail, head, grb->aval_size);

	return atomic_read(&grb->eof) ? read_bytes : size;
}

int get_rb_free_size(void)
{
	int32_t tail = atomic_read(&grb->wr_index);
	int32_t head = atomic_read(&grb->rd_index);
	grb->aval_size = get_free_size(tail, head);
	return grb->aval_size;
}

int get_rb_avalible_size(void)
{
	return BUFFER_SIZE - 1 -  get_rb_free_size();
}

int get_rb_max_size(void)
{
	return BUFFER_SIZE - 1;
}

void rb_force_exit(void)
{
	pr_debug("rb force exit");
	atomic_set(&grb->exit, 1);
	atomic_set(&grb->buf_condition, 1);
	wake_up_interruptible(&grb->wait_q);
}

void rb_end(void)
{
	atomic_set(&grb->eof, 1);
}

int rb_shoule_exit(void)
{
	return atomic_read(&grb->eof) || atomic_read(&grb->exit);
}

int create_rb(void)
{
	int32_t tail;
	int32_t head;
	grb = kzalloc(sizeof(struct rb), GFP_KERNEL);
	if (grb == NULL) {
		goto err;;
	}
	grb->gbuffer = kzalloc(BUFFER_SIZE, GFP_KERNEL);
	if (grb->gbuffer == NULL) {
		goto err;
	}

	rb_init();

	init_waitqueue_head(&grb->wait_q);
	tail = atomic_read(&grb->wr_index);
	head = atomic_read(&grb->rd_index);
	grb->aval_size = get_free_size(tail, head);

	return 0;
err:
	if (grb)
		kfree(grb);
	if (grb->gbuffer)
		kfree(grb->gbuffer);
	return  -EPERM;
}

void rb_init(void)
{
	pr_debug("rb init");
	atomic_set(&grb->wr_index, 0);
	atomic_set(&grb->rd_index, 0);
	atomic_set(&grb->buf_condition, 0);
	atomic_set(&grb->exit, 0);
	atomic_set(&grb->eof, 0);
}

int release_rb(void)
{
	if (grb  != NULL) {
		if (grb->gbuffer) {
			 kfree(grb->gbuffer);
			 grb->gbuffer = NULL;
		}
		kfree(grb);
		grb  = NULL;
	}
	return 0;
}
