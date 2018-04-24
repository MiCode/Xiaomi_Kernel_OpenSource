Index
=====
	* Introduction
	* Software architecture and Integration details
	* STM proprietary libraries
	* More information
	* Copyright


Introduction
=========
The STM Android sensor Hardware Abstraction Layer (*HAL*) defines a standard interface for STM sensors allowing Android to be agnostic about low level driver implementation. The HAL library is packaged into modules (.so) file and loaded by the Android system at the appropriate time. For more information see [AOSP HAL Interface](https://source.android.com/devices/sensors/hal-interface.html) 

STM Sensor HAL is leaning on [Linux IIO framework](https://git.kernel.org/cgit/linux/kernel/git/torvalds/linux.git/tree/Documentation/iio) to gather data from sensor device drivers and to forward samples to the Android Framework

Currently supported sensors are:

### Inertial Module Unit (IMU):

> ASM330LHH

Software architecture and Integration details
=============

STM Sensor HAL is written in *C++* language using object-oriented design. For each hw sensor there is a custom class file (*Accelerometer.cpp*, *Gyroscope.cpp*) which extends the common base class (*SensorBase.cpp*).

Copy the HAL source code into *<AOSP_DIR\>/hardware/STMicroelectronics/SensorHAL_IIO* folder. During building process Android will include automatically the SensorHAL Android.mk.
In *<AOSP_DIR\>/device/<vendor\>/<board\>/device.mk* add package build information:

	PRODUCT_PACKAGES += sensors.{TARGET_BOARD_PLATFORM}

	Note: device.mk can not read $(TARGET_BOARD_PLATFORM) variable, read and replace the value from your BoardConfig.mk (e.g. PRODUCT_PACKAGES += sensors.msm8974 for Nexus 5)

To compile the SensorHAL_IIO just build AOSP source code from *$TOP* folder

	$ cd <AOSP_DIR>
	$ source build/envsetup.sh
	$ lunch <select target platform>
	$ make V=99

The compiled library will be placed in *<AOSP_DIR\>/out/target/product/<board\>/system/vendor/lib/hw/sensor.{TARGET_BOARD_PLATFORM}.so*

To configure sensor the Sensor HAL IIO use mm utility from HAL root folder

	$mm sensors-defconfig (default configuration)
or

	$mm sensors-menuconfig

Copyright
========
Copyright (C) 2017 STMicroelectronics

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
