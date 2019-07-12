#pragma once

#define ELLIPTIC_SYSFS_ENGINE_FOLDER "engine"
#define ELLIPTIC_SYSFS_ROOT_FOLDER "elliptic"
#define ELLIPTIC_SYSFS_CALIBRATION_FILENAME "calibration"
#define ELLIPTIC_SYSFS_VERSION_FILENAME "version"


int elliptic_initialize_sysfs(void);
void elliptic_cleanup_sysfs(void);
