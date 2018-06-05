#ifndef _PARAM_ID_H_
#define _PARAM_ID_H_

#include <linux/string.h>
#include <linux/types.h>

#define PARAM_ID_LEN 0
#define PARAM_ID_OOM_ADJ 1
#define PARAM_ID_APP_UID 2
#define PARAM_ID_APP_PID 3
#define PARAM_ID_VALUE 4
#define PARAM_ID_SIZE 5
#define PARAM_ID_FD 6
#define PARAM_ID_RATE 7
#define PARAM_ID_SENSOR 8
#define PARAM_ID_WINDOW_TYPE 9
#define PARAM_ID_WINDOW_FLAG 10
#define PARAM_ID_RTIC_TYPE 11
#define PARAM_ID_RTIC_ASSET_ID 12
#define PARAM_ID_RTIC_ASSET_CATEGORY 13
#define PARAM_ID_RTIC_RESPONSE 14
#define NUM_PARAM_IDS 15

static inline int param_id_index(const char *param, const char *end)
{
	int id  = -1;
	int len = ((end != NULL) ? (end - param) : (int)strlen(param));

	if ((len == 3) && !memcmp(param, "len", 3))
		id = 0;
	else if ((len == 7) && !memcmp(param, "oom_adj", 7))
		id = 1;
	else if ((len == 7) && !memcmp(param, "app_uid", 7))
		id = 2;
	else if ((len == 7) && !memcmp(param, "app_pid", 7))
		id = 3;
	else if ((len == 5) && !memcmp(param, "value", 5))
		id = 4;
	else if ((len == 4) && !memcmp(param, "size", 4))
		id = 5;
	else if ((len == 2) && !memcmp(param, "fd", 2))
		id = 6;
	else if ((len == 4) && !memcmp(param, "rate", 4))
		id = 7;
	else if ((len == 6) && !memcmp(param, "sensor", 6))
		id = 8;
	else if ((len == 11) && !memcmp(param, "window_type", 11))
		id = 9;
	else if ((len == 11) && !memcmp(param, "window_flag", 11))
		id = 10;
	else if ((len == 9) && !memcmp(param, "rtic_type", 9))
		id = 11;
	else if ((len == 8) && !memcmp(param, "asset_id", 8))
		id = 12;
	else if ((len == 14) && !memcmp(param, "asset_category", 14))
		id = 13;
	else if ((len == 8) && !memcmp(param, "response", 8))
		id = 14;

	return id;
}

static inline const char *get_param_id_name(int id)
{
	const char *name = "?";

	switch (id) {
	case 0:
		name = "len";
		break;
	case 1:
		name = "oom_adj";
		break;
	case 2:
		name = "app_uid";
		break;
	case 3:
		name = "app_pid";
		break;
	case 4:
		name = "value";
		break;
	case 5:
		name = "size";
		break;
	case 6:
		name = "fd";
		break;
	case 7:
		name = "rate";
		break;
	case 8:
		name = "sensor";
		break;
	case 9:
		name = "window_type";
		break;
	case 10:
		name = "window_flag";
		break;
	case 11:
		name = "rtic_type";
		break;
	case 12:
		name = "asset_id";
		break;
	case 13:
		name = "asset_category";
		break;
	case 14:
		name = "response";
		break;
	}
	return name;
}

#endif /* _PARAM_ID_H_ */
