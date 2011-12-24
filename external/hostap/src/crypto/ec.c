/*
 * ec.c
 *
 */
#include "ec.h"
#include "wapi/wapi.h"
#include "crypto.h"


int wapi_construct_identity(X509 *cert, u8 *output)
{
	X509_NAME *subject = X509_get_subject_name(cert);
	X509_NAME *issuer  = X509_get_issuer_name(cert);
	ASN1_INTEGER *ser = X509_get_serialNumber(cert);
	u8 *p = output;
	int total_len = 0;
	int len;

	if (!subject || !issuer || !ser) {
		wpa_printf(MSG_ERROR, "WAPI: error retrieving "
				"fields from certificate");
		return -1;
	}

	len = i2d_X509_NAME(subject, &p);
	if (len < 0) {
		wpa_printf(MSG_ERROR, "WAPI: error encoding name");
		return -1;
	}
	total_len += len;

	len = i2d_X509_NAME(issuer, &p);
	if (len < 0) {
		wpa_printf(MSG_ERROR, "WAPI: error encoding issuer");
		return -1;
	}
	total_len += len;

	len = i2d_ASN1_INTEGER(ser, &p);
	if (len < 0) {
		wpa_printf(MSG_ERROR, "WAPI: error encoding serial");
		return -1;
	}
	total_len += len;

	return total_len;
}

int wapi_ecdsa_sign_frame(const u8 *key, const u8 *msg, size_t len, u8 *ecdsa)
{
	u8 sha_result[SHA256_DIGEST_SIZE];
	ECDSA_SIG *sig = NULL;
	EC_KEY *eckey = NULL;
	EC_GROUP *group = NULL;
	BIGNUM *priv_key;
	int retval = -1;
	int size;

	sha256_vector(1, &msg, &len, sha_result);
	wpa_hexdump(MSG_DEBUG, "WAPI sha256 result",
			sha_result, SHA256_DIGEST_SIZE);

	eckey = EC_KEY_new();
	group = wapi_ec_group();
	priv_key = BN_new();

	if (!eckey || !group || !priv_key) {
		wpa_printf(MSG_ERROR, "WAPI: error allocating "
				"openssl variables");
		goto end;
	}

	if (!BN_bin2bn(key, WAPI_EC_PRIV_LEN, priv_key) ||
			!EC_KEY_set_group(eckey, group) ||
			!EC_KEY_set_private_key(eckey, priv_key)) {
		wpa_printf(MSG_ERROR, "WAPI: error setting openssl keys");
		goto end;
	}

	sig = wapi_ecdsa_sign(sha_result, 32, NULL, NULL, eckey);
	if (!sig) {
		wpa_printf(MSG_ERROR, "WAPI: error creating signature");
		goto end;
	}

	size = BN_bn2bin(sig->r, ecdsa);
	size += BN_bn2bin(sig->s, ecdsa+size);

	if (size != WAPI_EC_PUB_LEN-1)
		wpa_printf(MSG_INFO, "WAPI: signature size is different (%d)",
				size);

	retval = 0;
end:
	if (eckey)
		EC_KEY_free(eckey);
	if (group)
		EC_GROUP_free(group);
	if (priv_key)
		BN_free(priv_key);

	return retval;
}

int wapi_encode_pubkey(EC_KEY *key, struct wapi_key_data *keydata)
{
	BN_CTX *context = NULL;
	int retval = -1;
	context = BN_CTX_new();
	if (!context)
		goto end;
	if (!EC_POINT_point2oct(
			EC_KEY_get0_group(key),
			EC_KEY_get0_public_key(key),
			EC_KEY_get_conv_form(key),
			keydata->content,
			WAPI_EC_PUB_LEN,
			context)) {
		wpa_printf(MSG_ERROR, "WAPI: error converting "
				"pubkey to binary");
		goto end;
	}
	keydata->len = 49;
	wpa_hexdump(MSG_DEBUG, "WAPI ecdh pubkey",
			keydata->content, WAPI_EC_PUB_LEN);

	retval = 0;
end:
	if (context)
		BN_CTX_free(context);
	return retval;
}

EC_GROUP *wapi_ec_group()
{
	EC_GROUP *group = NULL;
	EC_POINT *P = NULL;
	BN_CTX	 *ctx = NULL;
	BIGNUM	 *p = NULL, *a = NULL, *b = NULL;
	BIGNUM	 *x = NULL, *y = NULL, *order = NULL;
	int	 ok = 0;

	ctx = BN_CTX_new();
	if (ctx == NULL)
		goto err;

	p = BN_new();
	a = BN_new();
	b = BN_new();
	x = BN_new();
	y = BN_new();
	order = BN_new();
	if (p == NULL || a == NULL || b == NULL ||
			x == NULL || y == NULL || order == NULL)
		goto err;

	if (!BN_hex2bn(&p, WAPI_EC_CURVE_P) || !BN_hex2bn(&a, WAPI_EC_CURVE_A)
			|| !BN_hex2bn(&b, WAPI_EC_CURVE_B)) {
		goto err;
	}

	group = EC_GROUP_new_curve_GFp(p, a, b, ctx);
	if (group == NULL)
		goto err;

	P = EC_POINT_new(group);
	if (P == NULL)
		goto err;

	if (!BN_hex2bn(&x, WAPI_EC_CURVE_X) || !BN_hex2bn(&y, WAPI_EC_CURVE_Y))
		goto err;

	if (!EC_POINT_set_affine_coordinates_GFp(group, P, x, y, ctx))
		goto err;

	if (!BN_hex2bn(&order, WAPI_EC_CURVE_N) ||
			!BN_set_word(x, WAPI_EC_CURVE_H))
		goto err;

	if (!EC_GROUP_set_generator(group, P, order, x))
		goto err;

	ok = 1;
err:
	if (!ok) {
		EC_GROUP_free(group);
		group = NULL;
	}
	if (P)
		EC_POINT_free(P);
	if (ctx)
		BN_CTX_free(ctx);
	if (p)
		BN_free(p);
	if (a)
		BN_free(a);
	if (b)
		BN_free(b);
	if (order)
		BN_free(order);
	if (x)
		BN_free(x);
	if (y)
		BN_free(y);

	return group;
}



int wapi_ecdsa_verify(const unsigned char *dgst, int dgst_len,
		const ECDSA_SIG *sig, EC_KEY *eckey)
{
	int ret = -1;
	BN_CTX   *ctx;
	BIGNUM   *order, *u1, *u2, *m, *X;
	EC_POINT *point = NULL;
	const EC_GROUP *group;
	const EC_POINT *pub_key;

	/* check input values */
	group = EC_KEY_get0_group(eckey);
	pub_key = EC_KEY_get0_public_key(eckey);
	if (eckey == NULL || group == NULL ||
	    pub_key == NULL || sig == NULL)
		return -1;

	ctx = BN_CTX_new();
	if (!ctx)
		return -1;

	BN_CTX_start(ctx);
	order = BN_CTX_get(ctx);
	u1    = BN_CTX_get(ctx);
	u2    = BN_CTX_get(ctx);
	m     = BN_CTX_get(ctx);
	X     = BN_CTX_get(ctx);
	if (!X)
		goto err;

	if (!EC_GROUP_get_order(group, order, ctx))
		goto err;

	if (BN_is_zero(sig->r)          || BN_is_negative(sig->r) ||
	    BN_ucmp(sig->r, order) >= 0 || BN_is_zero(sig->s)  ||
	    BN_is_negative(sig->s)      || BN_ucmp(sig->s, order) >= 0) {
		ret = 0;	/* signature is invalid */
		goto err;
	}
	/* calculate tmp1 = inv(S) mod order */
	if (!BN_mod_inverse(u2, sig->s, order, ctx))
		goto err;

	/* digest -> m */
	if (!BN_bin2bn(dgst, dgst_len, m))
		goto err;
	/* u1 = m * tmp mod order */
	if (!BN_mod_mul(u1, m, u2, order, ctx))
		goto err;
	/* u2 = r * w mod q */
	if (!BN_mod_mul(u2, sig->r, u2, order, ctx))
		goto err;

	point = EC_POINT_new(group);
	if (point == NULL)
		goto err;

	if (!EC_POINT_mul(group, point, u1, pub_key, u2, ctx))
		goto err;

	if (!EC_POINT_get_affine_coordinates_GFp(group,
			point, X, NULL, ctx))
		goto err;

	if (!BN_nnmod(u1, X, order, ctx))
		goto err;
	/*  if the signature is correct u1 is equal to sig->r */
	ret = (BN_ucmp(u1, sig->r) == 0);
err:
	BN_CTX_end(ctx);
	BN_CTX_free(ctx);
	if (point)
		EC_POINT_free(point);
	return ret;
}



ECDSA_SIG *wapi_ecdsa_sign(const unsigned char *dgst, int dgst_len,
		const BIGNUM *in_kinv, const BIGNUM *in_r, EC_KEY *eckey)
{
	int     ok = 0;
	BIGNUM *kinv = NULL, *s, *m = NULL, *tmp = NULL, *order = NULL;
	const BIGNUM *ckinv;
	BN_CTX     *ctx = NULL;
	const EC_GROUP   *group;
	ECDSA_SIG  *ret;
	const BIGNUM *priv_key;

	group    = EC_KEY_get0_group(eckey);
	priv_key = EC_KEY_get0_private_key(eckey);

	if (group == NULL || priv_key == NULL)
		return NULL;

	ret = ECDSA_SIG_new();
	if (!ret)
		return NULL;
	s = ret->s;

	ctx = BN_CTX_new();
	order = BN_new();
	tmp = BN_new();
	m = BN_new();
	if (ctx == NULL || order == NULL ||
		tmp == NULL || m == NULL)
		goto err;

	if (!EC_GROUP_get_order(group, order, ctx))
		goto err;
	if (!BN_bin2bn(dgst, dgst_len, m))
		goto err;
	do {
		if (in_kinv == NULL || in_r == NULL) {
			if (!ECDSA_sign_setup(eckey, ctx, &kinv, &ret->r))
				goto err;
			ckinv = kinv;
		} else {
			ckinv  = in_kinv;
			if (BN_copy(ret->r, in_r) == NULL)
				goto err;
		}

		if (!BN_mod_mul(tmp, priv_key, ret->r, order, ctx))
			goto err;
		if (!BN_mod_add_quick(s, tmp, m, order))
			goto err;
		if (!BN_mod_mul(s, s, ckinv, order, ctx))
			goto err;
		if (BN_is_zero(s)) {
			/* if kinv and r have been supplied by the caller
			 * don't to generate new kinv and r values */
			if (in_kinv != NULL && in_r != NULL)
				goto err;
		} else
			/* s != 0 => we have a valid signature */
			break;
	} while (1);

	ok = 1;
err:
	if (!ok) {
		ECDSA_SIG_free(ret);
		ret = NULL;
	}
	if (ctx)
		BN_CTX_free(ctx);
	if (m)
		BN_clear_free(m);
	if (tmp)
		BN_clear_free(tmp);
	if (order)
		BN_free(order);
	if (kinv)
		BN_clear_free(kinv);
	return ret;
}



EVP_PKEY *wapi_ec_get_public_key(X509 *cert)
{
	EVP_PKEY *ret = NULL;
	long j;
	int type;
	const unsigned char *p;

	const unsigned char *cp;
	X509_ALGOR *a;
	X509_PUBKEY *key;

	if (!cert->cert_info || !cert->cert_info->key)
		goto err;

	key = cert->cert_info->key;

	if (key->public_key == NULL)
		goto err;

	type = OBJ_obj2nid(key->algor->algorithm);
	ret = EVP_PKEY_new();
	if (ret == NULL)
		goto err;
	ret->type = EVP_PKEY_type(type);

	/* the parameters must be extracted before the public key (ECDSA!) */
	wpa_printf(MSG_DEBUG, "WAPI: extracting pk");

	a = key->algor;

	if (ret->type == EVP_PKEY_EC) {
		if (a->parameter && (a->parameter->type == V_ASN1_SEQUENCE)) {
			/* type == V_ASN1_SEQUENCE
			 * => we have explicit parameters
			 * (e.g. parameters in the
			 * X9_62_EC_PARAMETERS-structure)
			 */
			ret->pkey.ec = EC_KEY_new();
			if (ret->pkey.ec == NULL)
				goto err;
			cp = p = a->parameter->value.sequence->data;
			j = a->parameter->value.sequence->length;
			if (!d2i_ECParameters(&ret->pkey.ec, &cp, (long)j))
				goto err;
		} else if (a->parameter &&
				(a->parameter->type == V_ASN1_OBJECT)) {
			/* type == V_ASN1_OBJECT => the parameters are given
			 * by an asn1 OID
			 */
			EC_KEY   *ec_key;
			EC_GROUP *group;

			if (ret->pkey.ec == NULL)
				ret->pkey.ec = EC_KEY_new();
			ec_key = ret->pkey.ec;
			if (ec_key == NULL)
				goto err;
			group = wapi_ec_group();
			if (group == NULL)
				goto err;
			EC_GROUP_set_asn1_flag(group, OPENSSL_EC_NAMED_CURVE);
			if (EC_KEY_set_group(ec_key, group) == 0)
				goto err;
			EC_GROUP_free(group);
		}
		/* the case implicitlyCA is currently not implemented */
		ret->save_parameters = 1;
	}

	p = key->public_key->data;
	j = key->public_key->length;
	if (!d2i_PublicKey(type, &ret, &p, (long)j))
		goto err;

	key->pkey = ret;
	CRYPTO_add(&ret->references, 1, CRYPTO_LOCK_EVP_PKEY);
	return ret;
err:
	wpa_printf(MSG_ERROR, "WAPI: error getting public key");
	if (ret)
		EVP_PKEY_free(ret);
	return NULL;
}


/**
 * returns 0 for valid signature, 1 for invalid, 2 for error
 */
int wapi_ecdsa_verify_frame(X509 *cert, const u8 *frame,
				size_t len, const u8 *signature)
{
	EVP_PKEY *ae_pkey = NULL;
	const u8 *pos = frame;
	u8 dgst[SHA256_DIGEST_SIZE];
	ECDSA_SIG *sig = NULL;
	int retval = 2;

	sha256_vector(1, &pos, &len, dgst);

	ae_pkey = wapi_ec_get_public_key(cert);
	if (!ae_pkey) {
		wpa_printf(MSG_ERROR, "WAPI: error reading public key");
		goto end;
	}

	sig = ECDSA_SIG_new();
	if (!sig) {
		wpa_printf(MSG_ERROR, "WAPI: error creating signature");
		goto end;
	}

	if (!BN_bin2bn(signature, WAPI_EC_PRIV_LEN, sig->r) ||
			!BN_bin2bn(signature + WAPI_EC_PRIV_LEN,
					WAPI_EC_PRIV_LEN, sig->s)) {
		wpa_printf(MSG_ERROR, "WAPI: error parsing signature");
		goto end;
	}

	retval = 1 - wapi_ecdsa_verify(dgst, SHA256_DIGEST_SIZE,
			sig, ae_pkey->pkey.ec);
end:
	if (ae_pkey)
		EVP_PKEY_free(ae_pkey);
	if (sig)
		ECDSA_SIG_free(sig);
	return retval;
}



/* extra X509 from a PEM byte array */
X509 *wapi_X509_from_PEM(char *buf, size_t len)
{
	X509 *x = NULL;
	BIO *bio = BIO_new(BIO_s_mem());

	if (!bio) {
		wpa_printf(MSG_ERROR, "WAPI: error creating BIO");
		goto end;
	}

	if (BIO_write(bio, buf, len) != len) {
		wpa_printf(MSG_ERROR, "WAPI: error writing to BIO");
		goto end;
	}

	x = PEM_read_bio_X509(bio, NULL, 0, NULL);
	if (!x) {
		wpa_printf(MSG_ERROR, "WAPI: error reading "
				"certificate from PEM");
		goto end;
	}

end:
	if (bio)
		BIO_free(bio);

	return x;
}

/* extract wapi private key from PEM byte array.
 * pkey should point to a 24 octet buffer to hold private key.
 * note that this is a hack which looks 7 bytes into the DER encoding.
 * this hack might not be suitable for other kinds of certificates */
int wapi_ec_extract_pkey_from_PEM(char *buf, size_t len, u8 *pkey)
{
	unsigned char *data = NULL;
	int retval = -1;
	long out_len;

	BIO *bio = BIO_new(BIO_s_mem());
	if (!bio) {
		wpa_printf(MSG_ERROR, "WAPI: error creating BIO");
		goto end;
	}

	if (BIO_write(bio, buf, len) != len) {
		wpa_printf(MSG_ERROR, "WAPI: error writing to BIO");
		goto end;
	}

	if (!PEM_bytes_read_bio(&data, &out_len, NULL,
			PEM_STRING_EVP_PKEY, bio, 0, NULL)) {
		wpa_printf(MSG_ERROR, "WAPI: error reading EC pkey section");
		goto end;
	}

	os_memcpy(pkey, data+7, WAPI_EC_PRIV_LEN);

	wpa_printf(MSG_INFO, "WAPI: private key extracted succesffuly");
	wpa_hexdump(MSG_DEBUG, "WAPI private key", pkey, WAPI_EC_PRIV_LEN);

	retval = 0;
end:
	if (bio)
		BIO_free(bio);
	if (data)
		OPENSSL_free(data);

	return retval;
}

/* encode a PEM certificate as DER */
int wapi_ec_X509_PEM_to_DER(char *buf, size_t len,
				size_t *output_len, u8 *output)
{
	X509 *x = NULL;
	int retval;
	u8 *p;

	x = wapi_X509_from_PEM(buf, len);
	if (!x) {
		wpa_printf(MSG_ERROR, "WAPI: error extracting certificate");
		return -1;
	}

	p = output;
	retval = i2d_X509(x, &p);
	if (retval < 0) {
		wpa_printf(MSG_ERROR, "WAPI: error encoding certificate");
		X509_free(x);
		return -1;
	}

	X509_free(x);
	*output_len = retval;

	wpa_printf(MSG_DEBUG, "WAPI: PEM encoded into %d octets", *output_len);
	return 0;
}
