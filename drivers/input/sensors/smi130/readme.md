# SMI130 sensor API

## Introduction

This package contains the Robert Bosch GmbH's SMI130 sensor driver (sensor API)

## Version

File            | Version | Date
----------------|---------|---------------
smi130.h        |  2.0.9  |   2018/06/21
smi130.c        |  2.0.9  |   2018/06/21
smi130_spi.c    |   1.3   |   2018/06/21
smi130_i2c.c    |   1.3   |   2018/06/21
smi130_driver.h |   1.3   |   2018/06/21
smi130_driver.c |   1.3   |   2018/06/21
bs_log.h        |         |   2018/06/21
bs_log.c        |         |   2018/06/21
boschclass.h    |  1.5.9  |   2018/06/21
boschclass.c    |  1.5.9  |   2018/06/21

## File information

* smi130.h : The head file of SMI130API
* smi130.c : Sensor Driver for SMI130 sensor
* smi130_spi.c : This file implements moudle function, which add the driver to SPI core.
* smi130_i2c.c : This file implements moudle function, which add the driver to I2C core.
* smi130_driver.h : The head file of SMI130 device driver core code
* smi130_driver.c : This file implements the core code of SMI130 device driver
* bs_log.h : The head file of BOSCH SENSOR LOG
* bs_log.c : The source file of BOSCH SENSOR LOG
* boschclass.h :
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

Special: Description of the Software:

This software module (hereinafter called "Software") and any
information on application-sheets (hereinafter called "Information") is
provided free of charge for the sole purpose to support your application
work.

As such, the Software is merely an experimental software, not tested for
safety in the field and only intended for inspiration for further development
and testing. Any usage in a safety-relevant field of use (like automotive,
seafaring, spacefaring, industrial plants etc.) was not intended, so there are
no precautions for such usage incorporated in the Software.

The Software is specifically designed for the exclusive use for Bosch
Sensortec products by personnel who have special experience and training. Do
not use this Software if you do not have the proper experience or training.

This Software package is provided as is and without any expressed or
implied warranties, including without limitation, the implied warranties of
merchantability and fitness for a particular purpose.

Bosch Sensortec and their representatives and agents deny any liability for
the functional impairment of this Software in terms of fitness, performance
and safety. Bosch Sensortec and their representatives and agents shall not be
liable for any direct or indirect damages or injury, except as otherwise
stipulated in mandatory applicable law.
The Information provided is believed to be accurate and reliable. Bosch
Sensortec assumes no responsibility for the consequences of use of such
Information nor for any infringement of patents or other rights of third
parties which may result from its use.

------------------------------------------------------------------------------
The following Product Disclaimer does not apply to the Kernel Driver 
This software program is licensed subject to the GNU General
Public License (GPL).Version 2,June 1991,
available at http://www.fsf.org/copyleft/gpl.html

Product Disclaimer

Common:

Assessment of Products Returned from Field

Returned products are considered good if they fulfill the specifications /
test data for 0-mileage and field listed in this document.

Engineering Samples

Engineering samples are marked with (e) or (E). Samples may vary from the
valid technical specifications of the series product contained in this
data sheet. Therefore, they are not intended or fit for resale to
third parties or for use in end products. Their sole purpose is internal
client testing. The testing of an engineering sample may in no way replace
the testing of a series product. Bosch assumes no liability for the use
of engineering samples. The purchaser shall indemnify Bosch from all claims
arising from the use of engineering samples.

Intended use

Provided that SMI130 is used within the conditions (environment, application,
installation, loads) as described in this TCD and the corresponding
agreed upon documents, Bosch ensures that the product complies with
the agreed properties. Agreements beyond this require
the written approval by Bosch. The product is considered fit for the intended
use when the product successfully has passed the tests
in accordance with the TCD and agreed upon documents.

It is the responsibility of the customer to ensure the proper application
of the product in the overall system/vehicle.

Bosch does not assume any responsibility for changes to the environment
of the product that deviate from the TCD and the agreed upon documents
as well as all applications not released by Bosch

The resale and/or use of products are at the purchaser’s own risk and
responsibility. The examination and testing of the SMI130
is the sole responsibility of the purchaser.

The purchaser shall indemnify Bosch from all third party claims
arising from any product use not covered by the parameters of
this product data sheet or not approved by Bosch and reimburse Bosch
for all costs and damages in connection with such claims.

The purchaser must monitor the market for the purchased products,
particularly with regard to product safety, and inform Bosch without delay
of all security relevant incidents.

Application Examples and Hints

With respect to any application examples, advice, normal values
and/or any information regarding the application of the device,
Bosch hereby disclaims any and all warranties and liabilities of any kind,
including without limitation warranties of
non-infringement of intellectual property rights or copyrights
of any third party.
The information given in this document shall in no event be regarded
as a guarantee of conditions or characteristics. They are provided
for illustrative purposes only and no evaluation regarding infringement
of intellectual property rights or copyrights or regarding functionality,
performance or error has been made.