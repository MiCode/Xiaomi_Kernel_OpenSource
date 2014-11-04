#ifndef __STREAM_BUFFER_GLOBAL_H_INCLUDED__
#define __STREAM_BUFFER_GLOBAL_H_INCLUDED__

typedef struct stream_buffer_s stream_buffer_t;
struct stream_buffer_s {
	unsigned	base;
	unsigned	limit;
	unsigned	top;
};

#endif /* __STREAM_BUFFER_PUBLIC_H_INCLUDED__ */

