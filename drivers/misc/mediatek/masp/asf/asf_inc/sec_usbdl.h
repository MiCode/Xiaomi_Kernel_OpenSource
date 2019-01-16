#ifndef SEC_USBDL_H
#define SEC_USBDL_H

/**************************************************************************
 * [S-USBDL]
 **************************************************************************/
/* S-USBDL Attribute */
#define ATTR_SUSBDL_DISABLE                 0x00
#define ATTR_SUSBDL_ENABLE                  0x11
#define ATTR_SUSBDL_ONLY_ENABLE_ON_SCHIP    0x22

/**************************************************************************
 * EXPORT FUNCTION
 **************************************************************************/
extern int sec_usbdl_enabled (void);

#endif /* SEC_USBDL_H */

