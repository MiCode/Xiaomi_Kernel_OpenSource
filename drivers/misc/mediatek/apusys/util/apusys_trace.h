/*
 * Copyright (C) 2018 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifdef CONFIG_FTRACE

#ifdef TRACE_LEN
#undef TRACE_LEN
#endif

#define TRACE_LEN 256

#ifdef TRACE_PUT
#undef TRACE_PUT
#endif

#define TRACE_PUTS(p) \
	do { \
		trace_puts(p);; \
	} while (0)

void trace_tag_begin(const char *format, ...);
void trace_tag_end(void);
void trace_tag_customer(const char *fmt, ...);

#else
static inline void trace_tag_begin(const char *format, ...)
{
}

static inline void trace_tag_end(void)
{
}

static inline void trace_tag_customer(const char *fmt, ...)
{
}
#endif

