#ifndef SEC_BOOT_H
#define SEC_BOOT_H

/**************************************************************************
 * [S-BOOT]
 **************************************************************************/
 
/* S-BOOT Attribute */
#define ATTR_SBOOT_DISABLE                  0x00
#define ATTR_SBOOT_ENABLE                   0x11
#define ATTR_SBOOT_ONLY_ENABLE_ON_SCHIP     0x22

/**************************************************************************
 * [SECURE BOOT CHECK] 
 **************************************************************************/
 
/* Note : this structure record all the partitions
          which should be verified by secure boot check */
#define AND_SEC_BOOT_CHECK_PART_SIZE        (90)
typedef struct 
{
    unsigned char                           name[9][10];
    
} AND_SECBOOT_CHECK_PART_T;

/* Note : partition name between preloader/DA and kernel mtd table may be different
          in order to reduce maintainence effort, secure boot update will apply
          following transfation table to correct mtd partition name */
#define MTD_SECCFG                     "seccnfg"
#define MTD_UBOOT                      "uboot"
#define MTD_LOGO                       "logo"
#define MTD_BOOTIMG                    "boot"
#define MTD_USER                       "userdata"
#define MTD_ANDSYSIMG                  "system"
#define MTD_RECOVERY                   "recovery"
#define MTD_SECRO                      "secstatic"

#define USIF_SECCFG                    "seccfg"
#define USIF_UBOOT                     "uboot"
#define USIF_LOGO                      "logo"
#define USIF_BOOTIMG                   "bootimg"
#define USIF_USER                      "userdata"
#define USIF_ANDSYSIMG                 "android"
#define USIF_RECOVERY                  "recovery"
#define USIF_SECRO                     "sec_ro"

#define PL_SECCFG                      "SECCFG"
#define PL_UBOOT                       "UBOOT"
#define PL_LOGO                        "LOGO"
#define PL_BOOTIMG                     "BOOTIMG"
#define PL_USER                        "USRDATA"
#define PL_ANDSYSIMG                   "ANDROID"
#define PL_RECOVERY                    "RECOVERY"
#define PL_SECRO                       "SEC_RO"

#define GPT_SECRO                      "secro"
#define GPT_PRELOADER                  "preloader"

/**************************************************************************
 * EXPORT FUNCTION
 **************************************************************************/
extern int masp_boot_init (void);
extern int sec_boot_enabled (void);
extern int sec_modem_auth_enabled (void);
extern int sec_schip_enabled (void);

#endif /* SEC_BOOT_H */

