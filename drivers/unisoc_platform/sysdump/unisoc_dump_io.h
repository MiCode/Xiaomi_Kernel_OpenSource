/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _UNISOC_DUMP_IO_
#define _UNISOC_DUMP_IO_

#if IS_ENABLED(CONFIG_UNISOC_DUMP_IO)
/**
 * for dump all requests status.
 */
extern void sprd_dump_io(void);
#else /*!CONFIG_UNISOC_DUMP_IO*/
static inline void sprd_dump_io(void) {}
#endif /*!CONFIG_UNISOC_DUMP_IO*/

#endif /*_UNISOC_DUMP_IO_*/

