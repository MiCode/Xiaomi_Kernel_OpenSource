#pragma once

#define MIUS_SYSFS_ENGINE_FOLDER "engine"
#define MIUS_SYSFS_ROOT_FOLDER "mius"
#define MIUS_SYSFS_CALIBRATION_FILENAME "calibration"
#define MIUS_SYSFS_VERSION_FILENAME "version"
#define MIUS_SYSFS_CALIBRATION_V2_FILENAME "calibration_v2"
#define MIUS_SYSFS_STATE_FILENAME "state"
#define MIUS_SYSFS_TAG_FILENAME "tag"
#define MIUS_SYSFS_OPMODE_FILENAME "opmode"
#define MIUS_SYSFS_OPMODE_FLAGS_FILENAME "opmode_flags"


int mius_initialize_sysfs(void);
void mius_cleanup_sysfs(void);
