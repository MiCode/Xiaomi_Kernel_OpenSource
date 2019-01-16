extern int lib_init_key (unsigned char *nKey, unsigned int nKey_len, unsigned char *eKey, unsigned int eKey_len);
extern int lib_verify (unsigned char* data_buf,  unsigned int data_len, unsigned char* sig_buf, unsigned int sig_len);
extern int lib_hash (unsigned char* data_buf,  unsigned int data_len, unsigned char* hash_buf, unsigned int hash_len);

int sec_init_key (unsigned char *nKey, unsigned int nKey_len, 
    unsigned char *eKey, unsigned int eKey_len)
{
    return lib_init_key(nKey, nKey_len, eKey, eKey_len);
}

int sec_hash(unsigned char *data_buf,  unsigned int data_len, 
    unsigned char *hash_buf, unsigned int hash_len)
{
    return lib_hash(data_buf, data_len, hash_buf, hash_len);
}

int sec_verify (unsigned char *data_buf,  unsigned int data_len, 
    unsigned char *sig_buf, unsigned int sig_len)
{
    return lib_verify(data_buf, data_len, sig_buf, sig_len);
}



