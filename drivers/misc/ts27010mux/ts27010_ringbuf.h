/*
 * simple ring buffer
 *
 * supports a concurrent reader and writer without locking
 */

#define SUPPORT_RINGBUF_DIRECT_PTR

struct ts27010_ringbuf {
	int len;
	int head;
	int tail;
	u8 buf[];
};

static inline struct ts27010_ringbuf *ts27010_ringbuf_alloc(int len)
{
	struct ts27010_ringbuf *rbuf;

#ifndef SUPPORT_RINGBUF_DIRECT_PTR
	rbuf = kzalloc(sizeof(*rbuf) + len, GFP_KERNEL);
#else /* SUPPORT_RINGBUF_DIRECT_PTR */
	rbuf = kzalloc(sizeof(*rbuf) + len * 2, GFP_KERNEL);
#endif /* SUPPORT_RINGBUF_DIRECT_PTR */
	if (rbuf == NULL)
		return NULL;

	rbuf->len = len;
	rbuf->head = 0;
	rbuf->tail = 0;

	return rbuf;
}

static inline void ts27010_ringbuf_free(struct ts27010_ringbuf *rbuf)
{
	kfree(rbuf);
}

static inline int ts27010_ringbuf_level(struct ts27010_ringbuf *rbuf)
{
	int level = rbuf->head - rbuf->tail;

	if (level < 0)
		level = rbuf->len + level;

	return level;
}

static inline int ts27010_ringbuf_room(struct ts27010_ringbuf *rbuf)
{
	return rbuf->len - ts27010_ringbuf_level(rbuf) - 1;
}

static inline u8 ts27010_ringbuf_peek(struct ts27010_ringbuf *rbuf, int i)
{
	return rbuf->buf[(rbuf->tail + i) % rbuf->len];
}

static inline int ts27010_ringbuf_consume(struct ts27010_ringbuf *rbuf,
					  int count)
{
	count = min(count, ts27010_ringbuf_level(rbuf));

	rbuf->tail = (rbuf->tail + count) % rbuf->len;

	return count;
}

static inline int ts27010_ringbuf_push(struct ts27010_ringbuf *rbuf, u8 datum)
{
	if (ts27010_ringbuf_room(rbuf) == 0)
		return 0;

	rbuf->buf[rbuf->head] = datum;
	rbuf->head = (rbuf->head + 1) % rbuf->len;

	return 1;
}

static inline int ts27010_ringbuf_write(struct ts27010_ringbuf *rbuf,
					const u8 *data, int len)
{
	int count = 0;
	int i;

	for (i = 0; i < len; i++)
		count += ts27010_ringbuf_push(rbuf, data[i]);

	return count;
}

#ifdef SUPPORT_RINGBUF_DIRECT_PTR
static inline u8 *ts27010_ringbuf_direct_ptr(struct ts27010_ringbuf *rbuf,
					     int i, int len)
{
	int index = (rbuf->tail + i) % rbuf->len;

	if (len <= 0 || len > ts27010_ringbuf_level(rbuf))
		return NULL;

	if (len + index > rbuf->len) {
		memcpy(&(rbuf->buf[rbuf->len]), &(rbuf->buf[0]),
		len + index - rbuf->len);
	}

	return &(rbuf->buf[index]);
}
#endif /* SUPPORT_RINGBUF_DIRECT_PTR */
