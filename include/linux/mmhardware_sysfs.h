#ifndef __MMHARDWARE_SYSFS_H__
#define __MMHARDWARE_SYSFS_H__

#define MM_HARDWARE_SYSFS_ROOT_FOLDER             "mm_hardware"
#define MM_HARDWARE_SYSFS_ADSP_FOLDER             "adsp"
#define MM_HARDWARE_SYSFS_CODEC_FOLDER            "codec"
#define MM_HARDWARE_SYSFS_PA_1_FOLDER             "pa1"
#define MM_HARDWARE_SYSFS_PA_2_FOLDER             "pa2"
#define MM_HARDWARE_SYSFS_PA_3_FOLDER             "pa3"
#define MM_HARDWARE_SYSFS_PA_4_FOLDER             "pa4"
#define MM_HARDWARE_SYSFS_PA_5_FOLDER             "pa5"
#define MM_HARDWARE_SYSFS_PA_6_FOLDER             "pa6"
#define MM_HARDWARE_SYSFS_PA_7_FOLDER             "pa7"
#define MM_HARDWARE_SYSFS_PA_8_FOLDER             "pa8"

enum hardware_id {
	MM_HW_FINE        = 0x1,
	MM_HW_ADSP        = 0x2,
	MM_HW_CODEC       = 0x4,
	MM_HW_PA_1        = 0x8,
	MM_HW_PA_2        = 0x10,
	MM_HW_PA_3        = 0x20,
	MM_HW_PA_4        = 0x40,
	MM_HW_PA_5        = 0x80,
	MM_HW_PA_6        = 0x100,
	MM_HW_PA_7        = 0x200,
	MM_HW_PA_8        = 0x400
};

struct mm_info {
	struct kobj_attribute k_attr;
	enum hardware_id mm_id;
	int on_register;
	int on_calibration;
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
	.on_calibration = 0,                                   \
}

/* create sysfs node */
#define MM_INFO(_id, _mm_name)      \
	struct mm_info _mm_name##_info = __MMHW(_id, _mm_name)

int mmhardware_initialize_sysfs(void);
void mmhardware_cleanup_sysfs(void);
int register_kobj_under_mmsysfs(enum hardware_id mm_id, const char *name);
int on_calibration_under_mmsysfs(enum hardware_id mm_id, const char *name);
#endif