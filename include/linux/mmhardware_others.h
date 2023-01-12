#ifndef __MMHARDWARE_OTHERS_H__
#define __MMHARDWARE_OTHERS_H__

#define MM_HARDWARE_SYSFS_OTHERS_FOLDER           "others"
#define MM_HARDWARE_SYSFS_AUDIOSWITCH_FOLDER      "audioswitch"
#define MM_HARDWARE_SYSFS_HAPTIC_1_FOLDER         "haptic1"
#define MM_HARDWARE_SYSFS_HAPTIC_2_FOLDER         "haptic2"

enum hardware_id {
	MM_HW_AS            = 0x800,
	MM_HW_HAPTIC_1      = 0x1000,
	MM_HW_HAPTIC_2      = 0x2000
};

struct mm_info {
	struct kobj_attribute k_attr;
	enum hardware_id mm_id;
	int on_register;
};

#define __MMHW(_id, _mm_name) {                            \
	.k_attr = {                                            \
				.attr  = {                                 \
						.name = __stringify(_mm_name),     \
						.mode = 0644,                      \
				},                                         \
				.show  = mm_register_show,                 \
				.store = mm_register_store,                \
	},                                                     \
	.mm_id = _id,             							   \
	.on_register = 0,                                      \
}

/* create sysfs node */
#define MM_INFO(_id, _mm_name)      \
	struct mm_info _mm_name##_info = __MMHW(_id, _mm_name)

int register_otherkobj_under_mmsysfs(enum hardware_id mm_id, const char *name);

#endif