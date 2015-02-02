/*
 * INTEL CONFIDENTIAL
 *
 * Copyright (C) 2010 - 2015 Intel Corporation.
 * All Rights Reserved.
 *
 * The source code contained or described herein and all documents
 * related to the source code ("Material") are owned by Intel Corporation
 * or licensors. Title to the Material remains with Intel
 * Corporation or its licensors. The Material contains trade
 * secrets and proprietary and confidential information of Intel or its
 * licensors. The Material is protected by worldwide copyright
 * and trade secret laws and treaty provisions. No part of the Material may
 * be used, copied, reproduced, modified, published, uploaded, posted,
 * transmitted, distributed, or disclosed in any way without Intel's prior
 * express written permission.
 *
 * No License under any patent, copyright, trade secret or other intellectual
 * property right is granted to or conferred upon you by disclosure or
 * delivery of the Materials, either expressly, by implication, inducement,
 * estoppel or otherwise. Any license under such intellectual property rights
 * must be express and approved by Intel in writing.
 */

#ifndef __IA_CSS_COMMON_IO_TYPES
#define __IA_CSS_COMMON_IO_TYPES

#define MAX_IO_DMA_CHANNELS 2

struct ia_css_common_io_config {
	unsigned base_address;
	unsigned width;
	unsigned height;
	unsigned stride;
	unsigned ddr_elems_per_word;
	unsigned dma_channel[MAX_IO_DMA_CHANNELS];
};

#endif /* __IA_CSS_COMMON_IO_TYPES */
