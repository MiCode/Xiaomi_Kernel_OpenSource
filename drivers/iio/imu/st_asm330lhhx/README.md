Index
=======
        * Introduction
        * Integration details
        * Self Test procedure
        * Copyright

Introduction
==============

This repository contains STMEMS ASM330LHHX LDD.

The ASM330LHHX is a clone os ASM330LHH LDD with support to load MLC/FSM ucf file, parse it and configure new iio device of type mlc/fsm

MLC/FSM iio devices can be loaded and removed at anytime also when MLC/FSM IIO device are configured and running

Follow some features of MLC/FSM for ASM330LHHX driver:
>	-	MLC/FSM devices can be loaded by using the *load_mlc* iio sysfs entry in MLC iio device. MLC firmware must be placed
>		in /lib/firmware/ with name st_asm330lhhx_mlc.bin
>		Please consider that ucf files can be loaded by using the SensorHAL Linux HAL with command --mlc which interprets the
>		ucf file and generates the binary to be loaded with the driver
>	-	MLC/FSM devices can be removed by using the *mlc_flush* iio sysfs entry in MLC iio device
>	-	Added support to remove driver module unregistering all MLC dynamic IIO device

Tested on rapsberry pi zero-w with the kernel v5.4 (md HiKey960 kernel v4.19rpi-5.3.y_asm330lhhx_mlc) and v5.4 (rpi-5.4.y)


Integration details
=====================

In order to explain how to integrate ASM330LHHX sensor in raspberry kernel, please consider the following integration steps:

### Source code integration

> * Copy driver source code into the target directory (e.g. *drivers/iio/imu*)
> * Edit related Kconfig (e.g. *drivers/iio/imu/Kconfig*) to include *ASM330LHHX* support:

>         source "drivers/iio/imu/st_asm330lhhx/Kconfig"

> * Edit related Makefile (e.g. *drivers/iio/imu/Makefile*) adding the following line:

>         obj-y += st_asm330lhhx/

> * Add custom events into *include/uapi/linux/iio/types.h*:

>         @@ -89,6 +98,7 @@ enum iio_event_type {
>                 IIO_EV_TYPE_THRESH_ADAPTIVE,
>                 IIO_EV_TYPE_MAG_ADAPTIVE,
>                 IIO_EV_TYPE_CHANGE,
>         +       IIO_EV_TYPE_FIFO_FLUSH,
>         };
>
>          @@ -96,6 +106,8 @@ enum iio_event_direction {
>                 IIO_EV_DIR_RISING,
>                 IIO_EV_DIR_FALLING,
>                 IIO_EV_DIR_NONE,
>         +       IIO_EV_DIR_FIFO_EMPTY,
>         +       IIO_EV_DIR_FIFO_DATA,
>         };

> * Add custom channel types *include/uapi/linux/iio/types.h* depending on the custom sensor implemented into driver:

>          @@ -43,6 +43,14 @@ enum iio_chan_type {
>                  IIO_ELECTRICALCONDUCTIVITY,
>                  IIO_COUNT,
>                  IIO_INDEX,
>          +       IIO_SIGN_MOTION,
>          +       IIO_STEP_DETECTOR,
>          +       IIO_STEP_COUNTER,
>          +       IIO_TILT,
>          +       IIO_TAP,
>          +       IIO_TAP_TAP,
>          +       IIO_WRIST_TILT_GESTURE,
>          +       IIO_GESTURE,
>                  IIO_GRAVITY,
>          };

### Device Tree configuration

> I2C example (based on Raspberry PI ZERO W):

>		&i2c1 {
>			pinctrl-names = "default";
>			pinctrl-0 = <&i2c1_pins>;
>			clock-frequency = <400000>;
>	   +		asm330lhhx@6b {
>	   +			compatible = "st,asm330lhhx";
>	   +			reg = <0x6b>;
>	   +			interrupt-parent = <&gpio>;
>	   +			interrupts = <26 IRQ_TYPE_LEVEL_HIGH>;
>	   +			st,int-pin = <1>;
>	   +			status = "okay";
>	   +            };
>		};

> SPI example (based on Raspberry PI ZERO W):

>               &spi0 {
>                       status = "ok";
>                       #address-cells = <0x1>;
>                       #size-cells = <0x0>;
>          +            asm330lhhx@0 {
>          +                    spi-max-frequency = <1000000>;
>          +                    compatible = "st,asm330lhhx";
>          +                    reg = <0>;
>          +                    interrupt-parent = <&gpio>;
>          +                    interrupts = <26 IRQ_TYPE_LEVEL_HIGH>;
>          +                    st,int-pin = <1>;
>          +                    status = "okay";
>          +            };
>               };

### Kernel configuration

Configure kernel with *make menuconfig* (alternatively use *make xconfig* or *make qconfig*)

>               Device Drivers  --->
>                       <M> Industrial I/O support  --->
>                               Inertial measurement units  --->
>				<M> STMicroelectronics ASM330LHHX sensor
>				[*]   Enable machine learning core
>				[*]   Enable wake-up irq


### Self Test procedure

Selftest procedure is embedded in the driver

For acc and gyro (iio device 0, 1) there are the following sysfs:

>	- selftest_available
>	- selftest

reading from selftest_available

>	$# > cat selftest_available
>	positive-sign, negative-sign

for starting positive-sign self test:

>	echo positive-sign > selftest

for check result:

>	cat selftest

>	[results are pass or fail]

for starting negative-sign self test:

>	echo negative-sign > selftest

for check result:

>	cat selftest

>	[results are pass or fail]

Copyright Driver
===========
Copyright (C) 2017 STMicroelectronics

This software is distributed under the GNU General Public License v2.0

[1]: https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/tree/Documentation/iio/iio_configfs.txt "IIO"
[2]: https://git.kernel.org/cgit/linux/kernel/git/torvalds/linux.git/tree/Documentation/i2c "I2C"
[3]: https://git.kernel.org/cgit/linux/kernel/git/torvalds/linux.git/tree/Documentation/spi "SPI"
[4]: https://git.kernel.org/cgit/linux/kernel/git/torvalds/linux.git/tree/Documentation/devicetree/bindings/interrupt-controller/interrupts.txt "interrupts"
