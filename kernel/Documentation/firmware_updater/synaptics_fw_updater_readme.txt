Use ADB (Android Debug Bridge) to do command-line reflash
- Power on device.
- Connect device to host via USB.
- Open command prompt on host and go to directory where adb, synaptics_fw_updater, and FW image (e.g. PR1234567.img) reside.
- Run "adb devices" to ensure connection with device.
- Run "adb root" to have root privileges.
- Run "adb push synaptics_fw_updater /data" to copy synaptics_fw_updater to /data directory on device.
- Run "adb push firmware.img /data" to copy firmware.img to /data directory on device.
- Run "adb shell chmod 777 /data/synaptics_fw_updater" to make synaptics_fw_updater executable.
- Run "adb shell /data/synaptics_fw_updater -b /data/PR1234567.img -f -v" to start reflash process.

Parameters
[-b {image_file}] - Name of image file
[-d {sysfs_entry}] - Path to sysfs entry of sensor
[-r] - Read config area
[-ui] - UI config area
[-pm] - Permanent config area
[-bl] - BL config area
[-dp] - Display config area
[-f] - Force reflash
[-v] - Verbose output

Procedures for checking whether to proceed with reflash
- If [-f] flag is set, proceed with reflash
- If device is in flash prog (bootloader) mode, proceed with reflash
- If PR number contained in name of new FW image is greater than PR number of FW on device, proceed with reflash.
- Otherwise, no reflash is performed

Usage examples
- Perform reflash using PR1234567.img regardless of PR number of FW on device
   synaptics_fw_updater -b PR1234567.img -f
- Perform reflash using PR1234567.img only if 1234567 is greater than PR number of FW on device.
   synaptics_fw_updater -b PR1234567.img
- Write UI config area from PR1234567.img (parsing UI config area from firmware image file)
   synaptics_fw_updater -b PR1234567.img -ui
- Write permanent config area from pmconfig.img (binary file containing permanent config data)
   synaptics_fw_updater -b pmconfig.img -pm
- Read UI config area
   synaptics_fw_updater -r -ui
- Read permanent config area
   synaptics_fw_updater -r -pm