/**
 * Copyright 2020 Mi
 * Copyright (C) 2021 XiaoMi, Inc.
 */

#include <linux/slab.h>
#include <linux/wait.h>
#include <linux/time.h>
#include <linux/jiffies.h>
#include <sound/asound.h>
#include <sound/soc.h>
#include <sound/control.h>

struct qdsp_data_state {
	int voice_model_state;
};
struct model_info {
	void *data;
	uint32_t len;
};
struct voice_sound_model {
	dma_addr_t  phys;
	void  *data;
	size_t  size; /* size of buffer */
	struct dma_buf  *dma_buf;
	uint32_t  mem_map_handle;
};

unsigned int send_data_add_component_controls(void *component);

