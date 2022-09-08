NOTE
=======

This repository contains an application specific driver version and may not reflect recent update.
For up-to-date driver version, please refer to the following repository:
[STMems_Linux_IIO_drivers](https://github.com/STMicroelectronics/STMems_Linux_IIO_drivers)

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
This repository contains asm330lhh IMU STMicroelectronics MEMS sensor linux driver support for kernel version 4.19.

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

This software is distributed under the GNU General Public License v2.0

[1]: https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/tree/Documentation/iio/iio_configfs.txt "IIO"
[2]: https://git.kernel.org/cgit/linux/kernel/git/torvalds/linux.git/tree/Documentation/i2c "I2C"
[3]: https://git.kernel.org/cgit/linux/kernel/git/torvalds/linux.git/tree/Documentation/spi "SPI"
[4]: https://git.kernel.org/cgit/linux/kernel/git/torvalds/linux.git/tree/Documentation/devicetree/bindings/interrupt-controller/interrupts.txt "interrupts"
