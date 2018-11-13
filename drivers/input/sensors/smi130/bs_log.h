/*!
 * @section LICENSE
 * (C) Copyright 2011~2016 Bosch Sensortec GmbH All Rights Reserved
 *
 * (C) Modification Copyright 2018 Robert Bosch Kft  All Rights Reserved
 *
 * This software program is licensed subject to the GNU General
 * Public License (GPL).Version 2,June 1991,
 * available at http://www.fsf.org/copyleft/gpl.html
 *
 * Special: Description of the Software:
 *
 * This software module (hereinafter called "Software") and any
 * information on application-sheets (hereinafter called "Information") is
 * provided free of charge for the sole purpose to support your application
 * work. 
 *
 * As such, the Software is merely an experimental software, not tested for
 * safety in the field and only intended for inspiration for further development 
 * and testing. Any usage in a safety-relevant field of use (like automotive,
 * seafaring, spacefaring, industrial plants etc.) was not intended, so there are
 * no precautions for such usage incorporated in the Software.
 * 
 * The Software is specifically designed for the exclusive use for Bosch
 * Sensortec products by personnel who have special experience and training. Do
 * not use this Software if you do not have the proper experience or training.
 * 
 * This Software package is provided as is and without any expressed or
 * implied warranties, including without limitation, the implied warranties of
 * merchantability and fitness for a particular purpose.
 * 
 * Bosch Sensortec and their representatives and agents deny any liability for
 * the functional impairment of this Software in terms of fitness, performance
 * and safety. Bosch Sensortec and their representatives and agents shall not be
 * liable for any direct or indirect damages or injury, except as otherwise
 * stipulated in mandatory applicable law.
 * The Information provided is believed to be accurate and reliable. Bosch
 * Sensortec assumes no responsibility for the consequences of use of such
 * Information nor for any infringement of patents or other rights of third
 * parties which may result from its use.
 * 
 *------------------------------------------------------------------------------
 * The following Product Disclaimer does not apply to the BSX4-HAL-4.1NoFusion Software 
 * which is licensed under the Apache License, Version 2.0 as stated above.  
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Product Disclaimer
 *
 * Common:
 *
 * Assessment of Products Returned from Field
 *
 * Returned products are considered good if they fulfill the specifications / 
 * test data for 0-mileage and field listed in this document.
 *
 * Engineering Samples
 * 
 * Engineering samples are marked with (e) or (E). Samples may vary from the
 * valid technical specifications of the series product contained in this
 * data sheet. Therefore, they are not intended or fit for resale to
 * third parties or for use in end products. Their sole purpose is internal
 * client testing. The testing of an engineering sample may in no way replace
 * the testing of a series product. Bosch assumes no liability for the use
 * of engineering samples. The purchaser shall indemnify Bosch from all claims
 * arising from the use of engineering samples.
 *
 * Intended use
 *
 * Provided that SMI130 is used within the conditions (environment, application,
 * installation, loads) as described in this TCD and the corresponding
 * agreed upon documents, Bosch ensures that the product complies with
 * the agreed properties. Agreements beyond this require
 * the written approval by Bosch. The product is considered fit for the intended
 * use when the product successfully has passed the tests
 * in accordance with the TCD and agreed upon documents.
 *
 * It is the responsibility of the customer to ensure the proper application
 * of the product in the overall system/vehicle.
 *
 * Bosch does not assume any responsibility for changes to the environment
 * of the product that deviate from the TCD and the agreed upon documents 
 * as well as all applications not released by Bosch
  *
 * The resale and/or use of products are at the purchaser’s own risk and 
 * responsibility. The examination and testing of the SMI130 
 * is the sole responsibility of the purchaser.
 *
 * The purchaser shall indemnify Bosch from all third party claims 
 * arising from any product use not covered by the parameters of 
 * this product data sheet or not approved by Bosch and reimburse Bosch 
 * for all costs and damages in connection with such claims.
 *
 * The purchaser must monitor the market for the purchased products,
 * particularly with regard to product safety, and inform Bosch without delay
 * of all security relevant incidents.
 *
 * Application Examples and Hints
 *
 * With respect to any application examples, advice, normal values
 * and/or any information regarding the application of the device,
 * Bosch hereby disclaims any and all warranties and liabilities of any kind,
 * including without limitation warranties of
 * non-infringement of intellectual property rights or copyrights
 * of any third party.
 * The information given in this document shall in no event be regarded 
 * as a guarantee of conditions or characteristics. They are provided
 * for illustrative purposes only and no evaluation regarding infringement
 * of intellectual property rights or copyrights or regarding functionality,
 * performance or error has been made.
 *
 * @filename bs_log.h
 * @date     "Sat Oct 11 16:12:16 2014 +0800"
 * @Modification Date 2018/08/28 18:20
 * @id       "762cc9e"
 *
 * @brief
 * The head file of BOSCH SENSOR LOG
*/

#ifndef __BS_LOG_H
#define __BS_LOG_H

#include <linux/kernel.h>

/*! @ trace functions
 @{*/
/*! ERROR LOG LEVEL */
#define LOG_LEVEL_E 3
/*! NOTICE LOG LEVEL */
#define LOG_LEVEL_N 5
/*! INFORMATION LOG LEVEL */
#define LOG_LEVEL_I 6
/*! DEBUG LOG LEVEL */
#define LOG_LEVEL_D 7
/*! DEBUG_FWDL LOG LEVEL */
#define LOG_LEVEL_DF 10
/*! DEBUG_DATA LOG LEVEL */
#define LOG_LEVEL_DA 15
/*! ALL LOG LEVEL */
#define LOG_LEVEL_A 20

#ifndef MODULE_TAG
/*! MODULE TAG DEFINATION */
#define MODULE_TAG "<BS_LOG>"
#endif

#ifndef LOG_LEVEL
/*! LOG LEVEL DEFINATION */
#define LOG_LEVEL LOG_LEVEL_I
#endif

#ifdef BOSCH_DRIVER_LOG_FUNC
	#ifdef BSLOG_VAR_DEF
		uint8_t debug_log_level = LOG_LEVEL;
	#else
		extern uint8_t debug_log_level;
	#endif

	/*! print error message */
	#define PERR(fmt, args...) do\
	{\
		if (debug_log_level >= LOG_LEVEL_E)\
			printk(KERN_INFO "\n" "[E]" KERN_ERR MODULE_TAG \
				"<%s><%d>" fmt "\n", __func__, __LINE__, ##args);\
	} while (0)

	/*! print notice message */
	#define PNOTICE(fmt, args...) do\
	{\
		if (debug_log_level >= LOG_LEVEL_N)\
			printk(KERN_INFO "\n" "[N]" KERN_NOTICE MODULE_TAG \
				"<%s><%d>" fmt "\n", __func__, __LINE__, ##args);\
	} while (0)

	/*! print information message */
	#define PINFO(fmt, args...) do\
	{\
		if (debug_log_level >= LOG_LEVEL_I)\
			printk(KERN_INFO "\n" "[I]" KERN_INFO MODULE_TAG \
				"<%s><%d>" fmt "\n", __func__, __LINE__, ##args);\
	} while (0)

	/*! print debug message */
	#define PDEBUG(fmt, args...) do\
	{\
		if (debug_log_level >= LOG_LEVEL_D)\
			printk(KERN_INFO "\n" "[D]" KERN_DEBUG MODULE_TAG \
				"<%s><%d>" fmt "\n", __func__, __LINE__, ##args);\
	} while (0)

	/*! print debug fw download message */
	#define PDEBUG_FWDL(fmt, args...) do\
	{\
		if (debug_log_level >= LOG_LEVEL_DF)\
			printk(KERN_INFO "\n" "[DF]" KERN_DEBUG MODULE_TAG \
				"<%s><%d>" fmt "\n", __func__, __LINE__, ##args);\
	} while (0)

	/*! print debug data log message */
	#define PDEBUG_DLOG(fmt, args...) do\
	{\
		if (debug_log_level >= LOG_LEVEL_DA)\
			printk(KERN_INFO "\n" "[DA]" KERN_DEBUG MODULE_TAG \
				"<%s><%d>" fmt "\n", __func__, __LINE__, ##args);\
	} while (0)

	void set_debug_log_level(uint8_t level);
	uint8_t get_debug_log_level(void);

#else

	#if (LOG_LEVEL >= LOG_LEVEL_E)
	/*! print error message */
	#define PERR(fmt, args...) \
		printk(KERN_INFO "\n" "[E]" KERN_ERR MODULE_TAG \
		"<%s><%d>" fmt "\n", __func__, __LINE__, ##args)
	#else
	/*! invalid message */
	#define PERR(fmt, args...)
	#endif

	#if (LOG_LEVEL >= LOG_LEVEL_N)
	/*! print notice message */
	#define PNOTICE(fmt, args...) \
		printk(KERN_INFO "\n" "[N]" KERN_NOTICE MODULE_TAG \
		"<%s><%d>" fmt "\n", __func__, __LINE__, ##args)
	#else
	/*! invalid message */
	#define PNOTICE(fmt, args...)
	#endif

	#if (LOG_LEVEL >= LOG_LEVEL_I)
	/*! print information message */
	#define PINFO(fmt, args...) printk(KERN_INFO "\n" "[I]" KERN_INFO MODULE_TAG \
		"<%s><%d>" fmt "\n", __func__, __LINE__, ##args)
	#else
	/*! invalid message */
	#define PINFO(fmt, args...)
	#endif

	#if (LOG_LEVEL >= LOG_LEVEL_D)
	/*! print debug message */
	#define PDEBUG(fmt, args...) printk(KERN_INFO "\n" "[D]" KERN_DEBUG MODULE_TAG \
		"<%s><%d>" fmt "\n", __func__, __LINE__, ##args)
	#else
	/*! invalid message */
	#define PDEBUG(fmt, args...)
	#endif

	#if (LOG_LEVEL >= LOG_LEVEL_DF)
	/*! print debug fw download message */
	#define PDEBUG_FWDL(fmt, args...) printk(KERN_INFO "\n" "[DF]" KERN_DEBUG MODULE_TAG \
		"<%s><%d>" fmt "\n", __func__, __LINE__, ##args)
	#else
	/*! invalid message */
	#define PDEBUG_FWDL(fmt, args...)
	#endif

	#if (LOG_LEVEL >= LOG_LEVEL_DA)
	/*! print debug data log message */
	#define PDEBUG_DLOG(fmt, args...) printk(KERN_INFO "\n" "[DA]" KERN_DEBUG MODULE_TAG \
		"<%s><%d>" fmt "\n", __func__, __LINE__, ##args)
	#else
	/*! invalid message */
	#define PDEBUG_DLOG(fmt, args...)
	#endif

	#define set_debug_log_level(level) {}
	#define get_debug_log_level() (LOG_LEVEL)

#endif

#endif/*__BS_LOG_H*/
/*@}*/
