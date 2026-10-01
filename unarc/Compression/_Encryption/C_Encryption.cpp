#include "C_Encryption.h"

extern "C" {
#define LTC_NO_CIPHERS
#define   LTC_BLOWFISH
#define   LTC_RIJNDAEL
#define     ENCRYPT_ONLY
#define   LTC_TWOFISH
#define   LTC_SERPENT
#define LTC_NO_HASHES
#define   LTC_SHA1
#define   LTC_SHA512
#define LTC_NO_MATH
#define LTC_NO_TEST
#include "ciphers/aes/aes.c"
#include "ciphers/blowfish.c"
#include "ciphers/twofish/twofish.c"
#include "ciphers/serpent.c"
#include "crypt/crypt_argchk.c"
#include "crypt/crypt_cipher_descriptor.c"
#include "crypt/crypt_cipher_is_valid.c"
#include "crypt/crypt_find_cipher.c"
#include "crypt/crypt_find_hash.c"
#include "crypt/crypt_find_prng.c"
#include "crypt/crypt_hash_descriptor.c"
#include "crypt/crypt_hash_is_valid.c"
#include "crypt/crypt_prng_descriptor.c"
#include "crypt/crypt_prng_is_valid.c"
#include "crypt/crypt_register_cipher.c"
#include "crypt/crypt_register_hash.c"
#include "crypt/crypt_register_prng.c"
#include "hashes/helper/hash_memory.c"
#include "hashes/sha1.c"
#include "hashes/sha2/sha512.c"
#include "mac/hmac/hmac_done.c"
#include "mac/hmac/hmac_init.c"
#include "mac/hmac/hmac_memory.c"
#include "mac/hmac/hmac_process.c"
#include "misc/error_to_string.c"
#include "misc/pkcs5/pkcs_5_2.c"
#include "misc/zeromem.c"
#include "modes/ctr/ctr_decrypt.c"
#include "modes/ctr/ctr_done.c"
#include "modes/ctr/ctr_encrypt.c"
#include "modes/ctr/ctr_start.c"
#include "modes/cfb/cfb_decrypt.c"
#include "modes/cfb/cfb_done.c"
#include "modes/cfb/cfb_encrypt.c"
#include "modes/cfb/cfb_start.c"
#include "prngs/fortuna.c"
}


/*-------------------------------------------------*/
/* ������������� ���������� ���������� LibTomCrypt */
/*-------------------------------------------------*/

// ��������������� ��� ���������� � ��������� ���������
int register_all()
{
    register_cipher (&aes_enc_desc);
    register_cipher (&blowfish_desc);
    register_cipher (&serpent_desc);
    register_cipher (&twofish_desc);
    register_hash (&sha1_desc);
    register_hash (&sha512_desc);
#ifndef LTC_NO_TEST
    CHECK (blowfish_test()==CRYPT_OK, (s,"blowfish_test failed!"));
//    CHECK (rijndael_test()==CRYPT_OK, (s,"rijndael_test failed!"));
    CHECK (serpent_test ()==CRYPT_OK, (s,"serpent_test failed!"));
    CHECK (twofish_test ()==CRYPT_OK, (s,"twofish_test failed!"));
    CHECK (sha1_test    ()==CRYPT_OK, (s,"sha1_test failed!"));
    CHECK (sha512_test  ()==CRYPT_OK, (s,"sha512_test failed!"));
//    CHECK (hmac_test    ()==CRYPT_OK, (s,"hmac_test failed!"));
//    CHECK (ctr_test     ()==CRYPT_OK, (s,"ctr_test failed!"));
//    CHECK (cfb_test     ()==CRYPT_OK, (s,"cfb_test failed!"));
#endif
    return 0;
}
int call_register_all = register_all();

// ������ ������ Fortuna PRNG
int fortuna_size (void)
{
    return sizeof(prng_state);
}


/*------------------------------------------------------*/
/* ���������� ��������� � ������� ���������� (CFB,CTR)  */
/*------------------------------------------------------*/

struct EncryptionMode
{
    int mode;
    symmetric_CTR ctr;
    symmetric_CFB cfb;

    EncryptionMode (int _mode) {mode = _mode;}

    char *name()
    {
        switch (mode) {
        case 0: return (char*)"ctr";
        case 1: return (char*)"cfb";
        default: return (char*)"unknown";
        }
    }

    int start (int cipher, BYTE *iv, BYTE *key, int keysize, int rounds)
    {
        switch (mode) {
        case 0: return ctr_start (cipher, iv, key, keysize, rounds, CTR_COUNTER_LITTLE_ENDIAN, &ctr);
        case 1: return cfb_start (cipher, iv, key, keysize, rounds, &cfb);
        default: return CRYPT_ERROR;
        }
    }

    int encrypt (BYTE *pt, BYTE *ct, int len)
    {
        switch (mode) {
        case 0: return ctr_encrypt(pt, ct, len, &ctr);
        case 1: return cfb_encrypt(pt, ct, len, &cfb);
        default: return CRYPT_ERROR;
        }
    }

    int decrypt (BYTE *pt, BYTE *ct, int len)
    {
        switch (mode) {
        case 0: return ctr_decrypt(pt, ct, len, &ctr);
        case 1: return cfb_decrypt(pt, ct, len, &cfb);
        default: return CRYPT_ERROR;
        }
    }

    int done()
    {
        switch (mode) {
        case 0: return ctr_done (&ctr);
        case 1: return cfb_done (&cfb);
        default: return CRYPT_ERROR;
        }
    }
};

// ����� ����� ������ ���������� �� ��� �����
int find_mode (char *name)
{
    if (strequ(name,"ctr"))  return  0;
    if (strequ(name,"cfb"))  return  1;
    else                     return -1;
}


/*-------------------------------------------------*/
/* ���������������� �������                        */
/*-------------------------------------------------*/

// ��������� ����� �� ������ � salt � �������������� numIterations �������� ����������� (PKCS5#2)
void Pbkdf2Hmac (const BYTE *pwd, int pwdSize, const BYTE *salt, int saltSize,
                 int numIterations, BYTE *key, int keySize)
{
    int hash = find_hash("sha512");
    unsigned long ulKeySize = keySize;
    pkcs_5_alg2 (pwd, pwdSize, salt, saltSize, numIterations, hash, key, &ulKeySize);
}

// ������������� ��� �������������� ����� ������, � ����������� �� �������� DoEncryption
int docrypt (enum TEncrypt DoEncryption, int cipher, int mode, BYTE *key, int keysize, int rounds, BYTE *iv,
             CALLBACK_FUNC *callback, void *auxdata)
{
    EncryptionMode encryptor(mode);
    encryptor.start (cipher, iv, key, keysize, rounds);

    int InSize = FREEARC_ERRCODE_NOT_ENOUGH_MEMORY;  // ���������� ����������� ���� ��� ��� ������
    int RemainderSize = 0;                           // �������������� ������� ����������� ����� (������ 0 � �������� ����������)
    BYTE* Buf = (BYTE*)malloc(LARGE_BUFFER_SIZE);    // ����� ��� ������
    if (!Buf)   goto Exit;                           // ����� ��� �������� ������

    while ( (InSize = callback ("read", Buf+RemainderSize, LARGE_BUFFER_SIZE-RemainderSize, auxdata)) >= 0 )  // ����� ��� ������ ������
    {
        if ((InSize+=RemainderSize)==0)     break;  // �����, ���� ������ ������ ���

        DoEncryption==ENCRYPT
          ? encryptor.encrypt(Buf, Buf, InSize)
          : encryptor.decrypt(Buf, Buf, InSize);

        int OutSize = InSize, x;
        if( (x=callback("write",Buf,OutSize,auxdata))<0 )   {InSize=x; break;}  // ����� ��� ������ ������
        RemainderSize = InSize-OutSize;
        // �������� �������������� ������� ������ � ������ ������
        if (RemainderSize>0)                memmove (Buf, Buf+OutSize, RemainderSize);
    }
Exit:
    encryptor.done();
    free (Buf);
    return InSize;  // ��������� ��� ������ ��� 0 ���� �� � �������
}


/*-------------------------------------------------*/
/* ���������� ������ ENCRYPTION_METHOD             */
/*-------------------------------------------------*/

// �����������, ������������� ���������� ������ ������ �������� �� ���������
ENCRYPTION_METHOD::ENCRYPTION_METHOD()
{
    cipher        = -1;
    mode          = -1;
    numIterations = 1000;
    rounds        = 0;
    keySize       = -1;
    strcpy(key,  "");
    strcpy(iv,   "");
    strcpy(salt, "");
    strcpy(code, "");
}

// ������������� �����, �������� �� ������� "encryption?", "KeySize" � "IVSize"
int ENCRYPTION_METHOD::doit (char *what, int param, void *data, CALLBACK_FUNC *callback)
{
         if (strequ (what, "encryption?"))    return 1;               // ��, ��� �������� ����������
    else if (strequ (what, "keySize"))        return keySize;         // ���������� ������ �����, ������������� � ������ ������ ������
    else if (strequ (what, "ivSize"))         return ivSize;          // ���������� ������ InitVector, ������������� � ������ ������ ������
    else if (strequ (what, "numIterations"))  return numIterations;   // ���������� ���������� ��������, ������������ ��� ��������� ����� �� password+salt
    else                                      return COMPRESSION_METHOD::doit (what, param, data, callback);  // �������� ��������� ������ ������������ ���������
}

static void decode16 (char *src, BYTE *dst)
{
    for( ; src[0] && src[1]; src+=2)
        *dst++ = char2int(src[0]) * 16 + char2int(src[1]);
}

static void encode16 (BYTE *src, int n, char *dst)
{
    static const char hex[] = "0123456789abcdef";
    for (int i = 0; i < n; i++)
    {
        dst[i*2]   = hex[src[i] >> 4];
        dst[i*2+1] = hex[src[i] & 15];
    }
    dst[n*2] = 0;
}


// ������� ����������
int ENCRYPTION_METHOD::decompress (CALLBACK_FUNC *callback, void *auxdata)
{
    BYTE key_bytes[MAXKEYSIZE];  decode16 (key, key_bytes);
    BYTE iv_bytes [MAXKEYSIZE];  decode16 (iv,  iv_bytes);
    return docrypt (DECRYPT, cipher, mode, key_bytes, strlen(key)/2, rounds, iv_bytes, callback, auxdata);
}

#ifndef FREEARC_DECOMPRESS_ONLY

extern "C" int systemRandomData (char *rand_buf, int rand_size);

extern "C" int GenerateEncryptionMethod (char *algorithm, char *password, char *out_method)
{
    if (!algorithm || !*algorithm)
        algorithm = (char*)"aes-256/ctr";
    COMPRESSION_METHOD *cm = ParseCompressionMethod (algorithm);
    if (!cm)
        return FREEARC_ERRCODE_INVALID_COMPRESSOR;
    if (cm->doit ((char*)"encryption?", 0, NULL, NULL) != 1)
    {
        delete cm;
        return FREEARC_ERRCODE_INVALID_COMPRESSOR;
    }
    ENCRYPTION_METHOD *e = (ENCRYPTION_METHOD*) cm;
    int keySize = e->keySize;
    int ivSize  = e->ivSize;
    if (keySize <= 0 || keySize > MAXKEYSIZE || ivSize <= 0 || ivSize > MAXKEYSIZE)
    {
        delete cm;
        return FREEARC_ERRCODE_INVALID_COMPRESSOR;
    }

    BYTE iv[MAXKEYSIZE], salt[MAXKEYSIZE], material[MAXKEYSIZE + 8];
    if (systemRandomData ((char*)iv, ivSize) != ivSize ||
        systemRandomData ((char*)salt, keySize) != keySize)
    {
        delete cm;
        return FREEARC_ERRCODE_READ;
    }

    int pwdSize = password ? (int)strlen (password) : 0;
    Pbkdf2Hmac ((const BYTE*)(password ? password : ""), pwdSize,
                salt, keySize, e->numIterations, material, keySize + 2);
    encode16 (material, keySize, e->key);
    encode16 (iv, ivSize, e->iv);
    encode16 (salt, keySize, e->salt);
    encode16 (material + keySize, 2, e->code);
    e->ShowCompressionMethod (out_method);
    delete cm;
    return FREEARC_OK;
}

int ENCRYPTION_METHOD::compress (CALLBACK_FUNC *callback, void *auxdata)
{
    BYTE key_bytes[MAXKEYSIZE];  decode16 (key, key_bytes);
    BYTE iv_bytes [MAXKEYSIZE];  decode16 (iv,  iv_bytes);
    return docrypt (ENCRYPT, cipher, mode, key_bytes, strlen(key)/2, rounds, iv_bytes, callback, auxdata);
}

// �������� � buf[MAX_METHOD_STRLEN] ������, ����������� ����� ������ � ��� ��������� (�������, �������� � parse_ENCRYPTION)
void ENCRYPTION_METHOD::ShowCompressionMethod (char *buf)
{
    sprintf (buf, "%s-%d/%s:n%d:r%d%s%s%s%s%s%s%s%s"
                                        , cipher_descriptor[cipher].name, keySize*8
                                        , EncryptionMode(mode).name()
                                        , numIterations
                                        , rounds
                                        , *key ?":k":"", key
                                        , *iv  ?":i":"", iv
                                        , *salt?":s":"", salt
                                        , *code?":c":"", code
                                        );
}

#endif  // !defined (FREEARC_DECOMPRESS_ONLY)

// ������������ ������ ���� ENCRYPTION_METHOD � ��������� ����������� ��������
// ��� ���������� NULL, ���� ��� ������ ����� ������ ��� �������� ������ � ����������
COMPRESSION_METHOD* parse_ENCRYPTION (char** parameters)
{
    int error = 0;  // ������� ����, ��� ��� ������� ���������� ��������� ������

    // ������ ��������� �����, ��������� split ������ ������
    char local_method[MAX_METHOD_STRLEN];
    strncopy (local_method, parameters[0], MAX_METHOD_STRLEN);

    // ��������� ������ ������ �� �������� 2 �����, ���������� ������ '/'
    // ��� ����� � ����� ���������� (��������, "aes/cfb")
    char *parts[3];
    split (local_method, '/', parts, 3);
    int mode    = parts[1]? find_mode(parts[1]) : 0;

    // ��������� ������ ������ �� �������� 2 �����, ���������� ������ '-'
    // ����� '-' ����� ���� ������ ������ ����� � ����� (��������, "aes-128")
    split (local_method, '-', parts, 3);

    int cipher  = find_cipher(parts[0]);
    int keySize = parts[1]? parseInt (parts[1], &error)/8 : 0;
    if (mode<0 || cipher<0 || error)   return NULL;   // ��� �� ����� ENCRYPTION

    ENCRYPTION_METHOD *p = new ENCRYPTION_METHOD;
    p->cipher  = cipher;
    p->mode    = mode;
    p->keySize = keySize? keySize : cipher_descriptor[cipher].max_key_length;
    p->ivSize  = cipher_descriptor[cipher].block_length;

    // �������� ��� ��������� ������ (��� ������ ������ ��� ������������� ������ ��� ������� ���������� ���������)
    while (*++parameters && !error)
    {
      char* param = *parameters;
      switch (*param) {                    // ���������, ���������� ��������
        case 'k':  strncopy (p->key,  param+1, sizeof (p->key));    continue;
        case 'i':  strncopy (p->iv,   param+1, sizeof (p->iv));     continue;
        case 's':  strncopy (p->salt, param+1, sizeof (p->salt));   continue;
        case 'c':  strncopy (p->code, param+1, sizeof (p->code));   continue;
        case 'n':  p->numIterations  = parseInt (param+1, &error);  continue;
        case 'r':  p->rounds         = parseInt (param+1, &error);  continue;
        default :  error=1;                                         continue;
      }
    }
    if (error)  {delete p; return NULL;}  // ������ ��� �������� ���������� ������
    return p;
}

static int ENCRYPTION_x = AddCompressionMethod (parse_ENCRYPTION);   // �������������� ������ ������ ENCRYPTION

extern "C" int InjectDecryptionKey (char *method, char *password, char *out_method)
{
    if (!method || !out_method)
        return FREEARC_ERRCODE_INVALID_COMPRESSOR;

    COMPRESSION_METHOD *cm = ParseCompressionMethod (method);
    if (!cm)
    {
        strncopy (out_method, method, MAX_METHOD_STRLEN);
        return FREEARC_OK;
    }
    if (cm->doit ((char*)"encryption?", 0, NULL, NULL) != 1)
    {
        delete cm;
        strncopy (out_method, method, MAX_METHOD_STRLEN);
        return FREEARC_OK;
    }

    ENCRYPTION_METHOD *e = (ENCRYPTION_METHOD*) cm;
    if (e->key[0])
    {
        delete cm;
        strncopy (out_method, method, MAX_METHOD_STRLEN);
        return FREEARC_OK;
    }
    if (!password)
    {
        delete cm;
        return FREEARC_ERRCODE_GENERAL;
    }
    int saltSize = (int) strlen (e->salt) / 2;
    int keySize  = e->keySize > 0 ? e->keySize : saltSize;
    if (keySize <= 0 || keySize > MAXKEYSIZE || saltSize <= 0 || saltSize > MAXKEYSIZE)
    {
        delete cm;
        return FREEARC_ERRCODE_INVALID_COMPRESSOR;
    }

    BYTE salt[MAXKEYSIZE], material[MAXKEYSIZE + 8], code[8];
    decode16 (e->salt, salt);
    Pbkdf2Hmac ((const BYTE*) password, (int) strlen (password),
                salt, saltSize, e->numIterations, material, keySize + 2);
    if (e->code[0])
    {
        decode16 (e->code, code);
        if (memcmp (material + keySize, code, 2) != 0)
        {
            delete cm;
            return FREEARC_ERRCODE_BAD_COMPRESSED_DATA;
        }
    }

    char keyhex[MAXKEYSIZE * 2 + 1];
    encode16 (material, keySize, keyhex);
    delete cm;
    int n = snprintf (out_method, MAX_METHOD_STRLEN, "%s:k%s", method, keyhex);
    if (n < 0 || n >= MAX_METHOD_STRLEN)
        return FREEARC_ERRCODE_INVALID_COMPRESSOR;
    return FREEARC_OK;
}

