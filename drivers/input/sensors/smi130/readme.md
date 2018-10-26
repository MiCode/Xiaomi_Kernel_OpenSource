# SMI130 sensor API
## Introduction
This package contains the Robert Bosch GmbH's SMI130 sensor driver (sensor API)

## Version
File                 | Version | Date
---------------------|---------|---------------
smi130.h             |  2.0.9  |   2018/08/28
smi130.c             |  2.0.9  |   2018/08/28
smi130_spi.c         |   1.3   |   2018/08/28
smi130_i2c.c         |   1.3   |   2018/08/28
smi130_gyro_driver.c |   1.5.9 |   2018/08/28
smi130_gyro.c        |   1.5   |   2018/08/28
smi130_gyro.h        |   1.5   |   2018/08/28
smi130_driver.h      |   1.3   |   2018/08/28
smi130_driver.c      |   1.3   |   2018/08/28
smi130_acc.c         |   2.1.2 |   2018/08/28
bs_log.h             |         |   2018/08/28
bs_log.c             |         |   2018/08/28
boschcalss.h         |  1.5.9  |   2018/08/28
boschclass.c         |  1.5.9  |   2018/08/28



## File information
* smi130.h : The head file of SMI130API
* smi130.c : Sensor Driver for SMI130 sensor
* smi130_spi.c : This file implements moudle function, which add the driver to SPI core.
* smi130_i2c.c : This file implements moudle function, which add the driver to I2C core.
* smi130_driver.h : The head file of SMI130 device driver core code
* smi130_driver.c : This file implements the core code of SMI130 device driver
* bs_log.h : The head file of BOSCH SENSOR LOG
* bs_log.c : The source file of BOSCH SENSOR LOG
* boschcalss.h :
* boschclass.c :


## Supported sensor interface
* SPI 4-wire
* I2C

## Copyright

Copyright (C) 2016 - 2017 Bosch Sensortec GmbH
Modification Copyright (C) 2018 Robert Bosch Kft  All Rights Reserved

This software program is licensed subject to the GNU General
Public License (GPL).Version 2,June 1991,
available at http://www.fsf.org/copyleft/gpl.html