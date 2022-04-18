# **Berlin Driver Porting Guide**

## **Introduce**
Berlin series driver is currently support BerlinA(GT9897), BerlinB(GT9966, GT7986). And support I2C or SPI connection.

## **Driver source file prepare**
1. Move driver source code to $KERNEL_SRC/drivers/input/touchscree/
2. Change $KERNEL_SRC/drivers/input/touchscree/Makefile  
	Add this line to the Makefile
	```
	obj-y += goodix_berlin_driver/
	```
3. Change $KERNEL_SRC/drivers/input/touchscree/Kconfg  
	Add this line to the Kconfig
	```
	source "drivers/input/touchscreen/goodix_berlin_driver/Kconfig"
	```

## **Add device declaration in the board devicetree**
Please add Goodix touch device declaration info in the board devicetree, you can refer the Appendix goodix-ts-i2c-dtsi or goodix-ts-spi-dtsi  to see how to set deivce properties.

## **Build driver**
When build kernel you will see the following promt to let you confirm how to build the driver. This driver support built-in kernel or build as modules.

**Setting up in the menu:**
1. In the $KERNEL_SRC directory, exec `make menuconfig`, then select  
`Device Drivers ---> Input device support ---> Touchscreens --->`  
2. Find `Goodix berlin touchscreen` menu, you can select `<*>`(build in kernel) or `<M>`(build a module).
3. Enter `Goodix berlin touchscreen`, you can see `support SPI bus connection` item. If 
you are on SPI connection, select `<*>`, or on I2C connection.

**Setting up in the defconfig file:**
1. Add the following in you defconfig file.
	```
	CONFIG_TOUCHSCREEN_GOODIX_BRL=y
	```
	or
	```
	CONFIG_TOUCHSCREEN_GOODIX_BRL=m
	```
2. If you are on SPI connection, add the following.
	```
	CONFIG_TOUCHSCREEN_GOODIX_BRL_SPI=y
	```

## **Appendix**

**goodix-ts-i2c-dtsi**

```dts
devicetree binding for Goodix i2c touchdriver
Required properties:
- compatible: device & driver matching.
	* for berlin series touch device, souch as "goodix,gt9897", "goodix,gt9966"

- reg: i2c client address, value can be 0x14 or 0x5d. please refer to datasheet.
- goodix,reset-gpio: reset gpio.
- goodix,irq-gpio: interrupt gpio. 
- goodix,irq-flags: irq trigger type config, value should be:
	       1 - rising edge,
	       2 - falling edge,
	       4 - high level,
	       5 - low level.
- goodix,panel-max-x: max resolution of x direction.
- goodix,panel-max-y: max resolution of y direction.
- goodix,panel-max-w: panel max width value.
- goodix,panel-max-p: pen device max pressure value.

Optional properties:
- goodix,avdd-name: set name of regulator.
- avdd-supply: power supply for the touch device.
  example of regulator:
	goodix,avdd-name = "avdd";
	avdd-supply = <&pm8916_l15>;
- iovdd-supply: power supply for digital io circuit
  example of regulator:
	goodix,iovdd-name = "iovdd";
	iovdd-supply = <&pm8916_l16>;
- goodix,pen-enable: set this property if you want support stylus.
	goodix,pen-enable;
- goodix,firmware-name: set firmware file name, if not configured, use the default name.
- goodix,config-name: set config file name, if not configured, use the default name.
Example 1:
goodix-berlin@5d {
	compatible = "goodix,gt9897";
	reg = <0x5d>;
	goodix,reset-gpio = <&msm_gpio 12 0x0>;
	goodix,irq-gpio = <&msm_gpio 13 0x2800>;
	goodix,irq-flags = <2>; /* 1:trigger rising, 2:trigger falling;*/
	goodix,panel-max-x = <720>;
	goodix,panel-max-y = <1280>;
	goodix,panel-max-w = <255>;
};

Example 2:
goodix-berlin@5d {
	compatible = "goodix,gt9966";
	goodix,avdd-name = "avdd";
	avdd-supply = <&pm8916_l15>;
	goodix,iovdd-name = "iovdd";
	iovdd-supply = <&pm8916_l16>;

	reg = <0x5d>;
	goodix,reset-gpio = <&msm_gpio 12 0x0>;
	goodix,irq-gpio = <&msm_gpio 13 0x2800>;
	goodix,irq-flags = <2>; /* 1:trigger rising, 2:trigger falling;*/
	goodix,panel-max-x = <720>;
	goodix,panel-max-y = <1280>;
	goodix,panel-max-w = <255>;
	goodix,panel-max-p = <4096>; /* max pressure that pen device supported */
	goodix,pen-enable; /* support active stylus device*/

	goodix,firmware-name = "goodix_firmware.bin";
	goodix,config-name = "goodix_cfg_group.bin";
};
```

**goodix-ts-spi-dtsi**

```dts
devicetree binding for Goodix spi touchdriver

Required properties:
- compatible: device & driver matching.
	* for berlin series touch device, souch as "goodix,gt9897T"

- spi-max-frequency: set spi transfer speed.
- reg: depend on CS gpio.
- goodix,reset-gpio: reset gpio.
- goodix,irq-gpio: interrupt gpio.
- goodix,irq-flags: irq trigger type config, value should be:
	       1 - rising edge,
	       2 - falling edge,
	       4 - high level,
	       5 - low level.
- goodix,panel-max-x: max resolution of x direction.
- goodix,panel-max-y: max resolution of y direction.
- goodix,panel-max-w: panel max width value.
- goodix,panel-max-p: pen device max pressure value.

Optional properties:
- goodix,avdd-name: set name of regulator.
- avdd-supply: power supply for the touch device.
  example of regulator:
	goodix,avdd-name = "avdd";
	avdd-supply = <&pm8916_l15>;
- iovdd-supply: power supply for digital io circuit
  example of regulator:
	goodix,iovdd-name = "iovdd";
	iovdd-supply = <&pm8916_l16>;
- goodix,pen-enable: set this property if you want support stylus.
	goodix,pen-enable;
- goodix,firmware-name: set firmware file name, if not configured, use the default name.
- goodix,config-name: set config file name, if not configured, use the default name.	
Example 1:
goodix-berlin@0 {
	compatible = "goodix,gt9897";
	reg = <0>;
	spi-max-frequency = <1000000>;
	goodix,reset-gpio = <&msm_gpio 12 0x0>;
	goodix,irq-gpio = <&msm_gpio 13 0x2800>;
	goodix,irq-flags = <2>; /* 1:trigger rising, 2:trigger falling;*/
	goodix,panel-max-x = <720>;
	goodix,panel-max-y = <1280>;
	goodix,panel-max-w = <255>;
};

Example 2:
goodix-berlin@0 {
	compatible = "goodix,gt9966S";
	reg = <0>;
	spi-max-frequency = <1000000>;

	goodix,avdd-name = "avdd";
	avdd-supply = <&pm8916_l15>;
	goodix,iovdd-name = "iovdd";
	iovdd-supply = <&pm8916_l16>;

	goodix,reset-gpio = <&msm_gpio 12 0x0>;
	goodix,irq-gpio = <&msm_gpio 13 0x2800>;
	goodix,irq-flags = <2>; /* 1:trigger rising, 2:trigger falling;*/
	goodix,panel-max-x = <720>;
	goodix,panel-max-y = <1280>;
	goodix,panel-max-w = <255>;
	goodix,panel-max-p = <4096>; /* max pressure that pen device supported */
	goodix,pen-enable; /* support active stylus device*/

	goodix,firmware-name = "goodix_firmware.bin";
	goodix,config-name = "goodix_cfg_group.bin";	
};
```

