/*
 * ec.h
 *
 */

#ifndef EC_H_
#define EC_H_


#include <openssl/ecdsa.h>
#include <openssl/ec.h>
#include <openssl/ecdh.h>
#include <openssl/bn.h>
#include <openssl/crypto.h>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/ecdsa.h>
#include <openssl/bio.h>
#include <openssl/pem.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>
#include <openssl/obj_mac.h>

#include "includes.h"
#include "common.h"


#define WAPI_EC_CURVE_P "BDB6F4FE3E8B1D9E0DA8C0D46F4C318CEFE4AFE3B6B8551F"
#define WAPI_EC_CURVE_A "BB8E5E8FBC115E139FE6A814FE48AAA6F0ADA1AA5DF91985"
#define WAPI_EC_CURVE_B "1854BEBDC31B21B7AEFC80AB0ECD10D5B1B3308E6DBF11C1"
#define WAPI_EC_CURVE_X "4AD5F7048DE709AD51236DE65E4D4B482C836DC6E4106640"
#define WAPI_EC_CURVE_Y "02BB3A02D4AAADACAE24817A4CA3A1B014B5270432DB27D2"
#define WAPI_EC_CURVE_N "BDB6F4FE3E8B1D9E0DA8C0D40FC962195DFAE76F56564677"
#define WAPI_EC_CURVE_H 1
#define WAPI_EC_PUB_LEN 49
#define WAPI_EC_PRIV_LEN 24
#define WAPI_EC_ECDSA_LEN 48

struct wapi_key_data;

EC_GROUP *wapi_ec_group();

int wapi_encode_pubkey(EC_KEY *key, struct wapi_key_data *keydata);

ECDSA_SIG *wapi_ecdsa_sign(const unsigned char *dgst, int dgst_len,
		const BIGNUM *in_kinv, const BIGNUM *in_r, EC_KEY *eckey);

int wapi_ecdsa_verify(const unsigned char *dgst, int dgst_len,
		const ECDSA_SIG *sig, EC_KEY *eckey);

int wapi_ecdsa_sign_frame(const u8 *key, const u8 *msg, size_t len, u8 *ecdsa);

int wapi_construct_identity(X509 *cert, u8 *output);

EVP_PKEY *wapi_ec_get_public_key(X509 *cert);

int wapi_ecdsa_verify_frame(X509 *cert, const u8 *frame,
		size_t len, const u8 *signature);

X509 *wapi_X509_from_PEM(char *buf, size_t len);
int wapi_ec_extract_pkey_from_PEM(char *buf, size_t len, u8 *pkey);
int wapi_ec_X509_PEM_to_DER(char *buf, size_t len,
				size_t *output_len, u8 *output);

#endif /* EC_H_ */
