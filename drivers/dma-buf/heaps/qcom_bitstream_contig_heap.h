/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2021 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef _QCOM_BITSTREAM_CONTIG_HEAP_H
#define _QCOM_BITSTREAM_CONTIG_HEAP_H

#ifdef CONFIG_QCOM_DMABUF_HEAPS_BITSTREAM_CONTIG
int qcom_add_bitstream_contig_heap(char *name);
#else
static int qcom_add_bitstream_contig_heap(char *name)
{
	return 0;
}
#endif

#endif /* _QCOM_BITSTREAM_CONTIG_HEAP_H */
