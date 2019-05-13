#pragma once

#define ELLIPTIC_SYSFS_ENGINE_FOLDER "engine"
#define ELLIPTIC_SYSFS_ROOT_FOLDER "elliptic"
#define ELLIPTIC_SYSFS_CALIBRATION_FILENAME "calibration"
#define ELLIPTIC_SYSFS_VERSION_FILENAME "version"
#define ELLIPTIC_SYSFS_CALIBRATION_V2_FILENAME "calibration_v2"
#define ELLIPTIC_SYSFS_STATE_FILENAME "state"
#define ELLIPTIC_SYSFS_TAG_FILENAME "tag"
#define ELLIPTIC_SYSFS_OPMODE_FILENAME "opmode"
#define ELLIPTIC_SYSFS_OPMODE_FLAGS_FILENAME "opmode_flags"


int elliptic_initialize_sysfs(void);
void elliptic_cleanup_sysfs(void);
