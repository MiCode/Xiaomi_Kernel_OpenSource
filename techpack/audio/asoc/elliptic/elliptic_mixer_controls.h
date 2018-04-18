#pragma once
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/soc.h>
#include <linux/types.h>


#define ELLIPTIC_OBJ_ID_CALIBRATION_DATA 1
#define ELLIPTIC_OBJ_ID_VERSION_INFO 2
#define ELLIPTIC_OBJ_ID_BRANCH_INFO 3

struct elliptic_engine_version_info {
	uint32_t major;
	uint32_t minor;
	uint32_t build;
	uint32_t revision;
};

struct elliptic_shared_data_block {
	uint32_t object_id;
	size_t size;
	void *buffer;
};

struct elliptic_shared_data_block *elliptic_get_shared_obj(uint32_t
	object_id);

