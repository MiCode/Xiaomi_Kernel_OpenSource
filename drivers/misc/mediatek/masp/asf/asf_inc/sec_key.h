#ifndef SEC_KEY_H
#define SEC_KEY_H

/**************************************************************************
 * [SEC-KEY ID]
 **************************************************************************/
#define ROM_INFO_SEC_KEY_ID              "AND_SECKEY_v"
#define ROM_INFO_SEC_KEY_VER             0x1


/**************************************************************************
 * [SEC-KEY FORNAT]
 **************************************************************************/
#define AND_SEC_KEY_SIZE                (592)

typedef struct {
    unsigned char                       m_id[16];
    unsigned int                        m_sec_key_ver;    

    /* rsa key */
    /* image auth key length is 256/2 = 128 */
    unsigned char                       img_auth_rsa_n[256];
    unsigned char                       img_auth_rsa_e[5];

    /* sml aes key */    
    unsigned char                       sml_aes_key[32];

    /* crypto seed */
    unsigned char                       crypto_seed[16];

    /* rsa key */
    /* sml auth key length is 256/2 = 128 */
    unsigned char                       sml_auth_rsa_n[256];
    unsigned char                       sml_auth_rsa_e[5];

} AND_SECKEY_T;

#endif /* SEC_KEY_H */

