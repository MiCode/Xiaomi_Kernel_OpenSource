Index
=======
	* Introduction
	* Driver Integration details
	* Android SensorHAL integration
	* Linux SensorHAL integration
	* More information
	* Copyright


Introduction
==============
This repository contains asm330lhh IMU STMicroelectronics MEMS sensor linux driver support for kernel version 4.14.

Data collected by asm330lhh STM sensor are pushed to userland through the kernel buffers of Linux IIO framework. User space applications can get sensor events by reading the related IIO devices created in the /dev directory (*/dev/iio{x}*). Please see [IIO][1] for more information.

Asm330lhh IMU STM MEMS sensor support *I2C/SPI* digital interface. Please refer to [I2C][2] and [SPI][3] for detailed documentation.

The STM Hardware Abstraction Layer (*HAL*) defines a standard interface for STM sensors allowing Android to be agnostic about low level driver implementation. The HAL library is packaged into modules (.so) file and loaded by the Android or Linux system at the appropriate time. For more information see [AOSP HAL Interface](https://source.android.com/devices/sensors/hal-interface.html) 

STM Sensor HAL is leaning on [Linux IIO framework](https://git.kernel.org/cgit/linux/kernel/git/torvalds/linux.git/tree/Documentation/iio) to gather data from sensor device drivers and to forward samples to the Android Framework

Driver Integration details
=====================

In order to explain how to integrate Asm330lhh IMU STM sensor into the kernel, please consider the following example

### Source code integration

> * Copy driver source code into your linux kernel target directory (e.g. *drivers/iio/imu*)
> * Edit related Kconfig (e.g. *drivers/iio/imu/Kconfig*) adding *ASM330LHH* support:

>         source "drivers/iio/imu/st_asm330lhh/Kconfig"

> * Edit related Makefile (e.g. *drivers/iio/imu/Makefile*) adding the following line:

>         obj-y += st_asm330lhh/

### Device Tree configuration

> To enable driver probing, add the asm330lhh node to the platform device tree as described below.

> **Required properties:**

> *- compatible*: "st,asm330lhh"

> *- reg*: the I2C address or SPI chip select the device will respond to

> *- interrupt-parent*: phandle to the parent interrupt controller as documented in [interrupts][4]

> *- interrupts*: interrupt mapping for IRQ as documented in [interrupts][4]
> 
>**Recommended properties for SPI bus usage:**

> *- spi-max-frequency*: maximum SPI bus frequency as documented in [SPI][3]
> 
> **Optional properties:**

> *- st,drdy-int-pin*: MEMS sensor interrupt line to use (default 1)

> I2C example (based on Raspberry PI 3):

>		&i2c0 {
>			status = "ok";
>			#address-cells = <0x1>;
>			#size-cells = <0x0>;
>			asm330lhh@6b {
>				compatible = "st,asm330lhh";
>				reg = <0x6b>;
>				interrupt-parent = <&gpio>;
>				interrupts = <26 IRQ_TYPE_EDGE_RISING>;
>		};

> SPI example (based on Raspberry PI 3):

>		&spi0 {
>			status = "ok";
>			#address-cells = <0x1>;
>			#size-cells = <0x0>;
>			asm330lhh@0 {
>				spi-max-frequency = <500000>;
>				compatible = "st,asm330lhh";
>				reg = <0>;
>				interrupt-parent = <&gpio>;
>				interrupts = <26 IRQ_TYPE_EDGE_RISING>;
>			};

### Kernel configuration

Configure kernel with *make menuconfig* (alternatively use *make xconfig* or *make qconfig*)
 
>		Device Drivers  --->
>			<M> Industrial I/O support  --->
>				Inertial measurement units  --->
>				<M>   STMicroelectronics ASM330LHH sensor  --->


Android SensorHAL integration
==============

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

    since Android 7
	$mm sensors-defconfig (default configuration)
	$mm sensors-menuconfig

    after Android 7
    make -f Makefile_config sensors-defconfig (default configuration)
    make -f Makefile_config sensors-menuconfig
    
Linux SensorHAL integration
==============

Linux Sensor HAL share the same source code of Android Sensor HAL. Before compiling the Linux Sensor HAL IIO
you need to follow the same procedure listed in previous chapter "Android SensorHAL integration"
To cross compile Linux Sensor HAL must export the following shell variables 

>   export AOSP_DIR=<AOSP_DIR>
>   export ARCH=<your target architecture>
>   export CROSS_COMPILE=<toolchain for your target>

then in *<AOSP_DIR\>/hardware/STMicroelectronics/SensorHAL_IIO* folder type
>   make

it will produce a SensorHAL.so file containing the library. 
In relative pat Documentation/LinuxHal/ there are some examples explaining how to use Linux Sensor HAL

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


More Information
=================
[http://st.com](http://st.com)

[https://git.kernel.org/cgit/linux/kernel/git/torvalds/linux.git/tree/Documentation/iio](https://git.kernel.org/cgit/linux/kernel/git/torvalds/linux.git/tree/Documentation/input)

[https://git.kernel.org/cgit/linux/kernel/git/torvalds/linux.git/tree/Documentation/i2c](https://git.kernel.org/cgit/linux/kernel/git/torvalds/linux.git/tree/Documentation/i2c)

[https://git.kernel.org/cgit/linux/kernel/git/torvalds/linux.git/tree/Documentation/spi](https://git.kernel.org/cgit/linux/kernel/git/torvalds/linux.git/tree/Documentation/spi)

[https://git.kernel.org/cgit/linux/kernel/git/torvalds/linux.git/tree/Documentation/devicetree/bings/interrupt-controller/interrupts.txt](https://git.kernel.org/cgit/linux/kernel/git/torvalds/linux.git/tree/Documentation/devicetree/bindings/interrupt-controller/interrupts.txt)


Copyright Driver
===========
Copyright (C) 2017 STMicroelectronics

This software is distributed under the GNU General Public License - see the accompanying COPYING file for more details.

[1]: https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/tree/Documentation/iio/iio_configfs.txt "IIO"
[2]: https://git.kernel.org/cgit/linux/kernel/git/torvalds/linux.git/tree/Documentation/i2c "I2C"
[3]: https://git.kernel.org/cgit/linux/kernel/git/torvalds/linux.git/tree/Documentation/spi "SPI"
[4]: https://git.kernel.org/cgit/linux/kernel/git/torvalds/linux.git/tree/Documentation/devicetree/bindings/interrupt-controller/interrupts.txt "interrupts"

Copyright SensorHAL
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
