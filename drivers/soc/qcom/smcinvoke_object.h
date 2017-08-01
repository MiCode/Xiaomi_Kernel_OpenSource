/* Copyright (c) 2016-2017, The Linux Foundation. All rights reserved.
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
#ifndef __SMCINVOKE_OBJECT_H
#define __SMCINVOKE_OBJECT_H

#include <linux/types.h>

#define object_op_METHOD_MASK   ((uint32_t)0x0000FFFFu)
#define object_op_RELEASE       (object_op_METHOD_MASK - 0)
#define object_op_RETAIN        (object_op_METHOD_MASK - 1)

#define object_counts_max_BI   0xF
#define object_counts_max_BO   0xF
#define object_counts_max_OI   0xF
#define object_counts_max_OO   0xF

/* unpack counts */

#define object_counts_num_BI(k)  ((size_t) (((k) >> 0) & object_counts_max_BI))
#define object_counts_num_BO(k)  ((size_t) (((k) >> 4) & object_counts_max_BO))
#define object_counts_num_OI(k)  ((size_t) (((k) >> 8) & object_counts_max_OI))
#define object_counts_num_OO(k)  ((size_t) (((k) >> 12) & object_counts_max_OO))
#define object_counts_num_buffers(k)	\
			(object_counts_num_BI(k) + object_counts_num_BO(k))

#define object_counts_num_objects(k)	\
			(object_counts_num_OI(k) + object_counts_num_OO(k))

/* Indices into args[] */

#define object_counts_index_BI(k)   0
#define object_counts_index_BO(k)		\
			(object_counts_index_BI(k) + object_counts_num_BI(k))
#define object_counts_index_OI(k)		\
			(object_counts_index_BO(k) + object_counts_num_BO(k))
#define object_counts_index_OO(k)		\
			(object_counts_index_OI(k) + object_counts_num_OI(k))
#define object_counts_total(k)		\
			(object_counts_index_OO(k) + object_counts_num_OO(k))


#endif /* __SMCINVOKE_OBJECT_H */
