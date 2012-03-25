/***************************************************************************
**+----------------------------------------------------------------------+**
**|                                ****                                  |**
**|                                ****                                  |**
**|                                ******o***                            |**
**|                          ********_///_****                           |**
**|                           ***** /_//_/ ****                          |**
**|                            ** ** (__/ ****                           |**
**|                                *********                             |**
**|                                 ****                                 |**
**|                                  ***                                 |**
**|                                                                      |**
**|     Copyright (c) 1998 - 2012 Texas Instruments Incorporated         |**
**|                        ALL RIGHTS RESERVED                           |**
**|                                                                      |**
**| Permission is hereby granted to licensees of Texas Instruments       |**
**| Incorporated (TI) products to use this computer program for the sole |**
**| purpose of implementing a licensee product based on TI products.     |**
**| No other rights to reproduce, use, or disseminate this computer      |**
**| program, whether in part or in whole, are granted.                   |**
**|                                                                      |**
**| TI makes no representation or warranties with respect to the         |**
**| performance of this computer program, and specifically disclaims     |**
**| any responsibility for any damages, special or consequential,        |**
**| connected with the use of this program.                              |**
**|                                                                      |**
**+----------------------------------------------------------------------+**/
/*
 * wapi.c
 *
 * Partly modified 2011 Sony Ericsson Mobile Communications AB.
 */


#include <sys/stat.h>
#include "utils/includes.h"
#include "wapi.h"
#include "crypto/sha256.h"
#include "crypto/sms4.h"
#include "utils/base64.h"
#include "keystore_get.h"


#define WAPI_IE_MIN_LEN 22    /* minimal size when all suites are present */


/* expansion strings to be used in key derivation process */
#define WAPI_EXPANSION_PSK_BK \
	"preshared key expansion for authentication and key negotiation"
#define WAPI_EXPANSION_PSK_BK_LEN 62

#define WAPI_EXPANSION_BK_UCAST \
	"pairwise key expansion for unicast and additional keys and nonce"
#define WAPI_EXPANSION_BK_UCAST_LEN 64

#define WAPI_EXPANSION_NMK \
	"multicast or station key expansion for station unicast and multicast and broadcast"
#define WAPI_EXPANSION_NMK_LEN 82

#define WAPI_EXPANSION_EC_BK "base key expansion for key and additional nonce"
#define WAPI_EXPANSION_EC_BK_LEN 47


static const u8 WAPI_CIPHER_SUITE_SMS4[] = { 0x00, 0x14, 0x72, 1 };
static const u8 WAPI_AUTH_SUITE_CERTIFICATE[] = { 0x00, 0x14, 0x72, 1 };
static const u8 WAPI_AUTH_SUITE_PSK[] = { 0x00, 0x14, 0x72, 2 };
static const u8 WAPI_ECDH_PARAMS[] = { 0x06, 0x09, 0x2A, 0x81, 0x1C,
					0xD7, 0x63, 0x01, 0x01, 0x02, 0x01 };
static const u8 WAPI_SIGNATURE_ALG[] = { 0x00, 0x10, 0x01, 0x01, 0x01, 0x00,
					0x0B, 0x06, 0x09, 0x2A, 0x81, 0x1C,
					0xD7, 0x63, 0x01, 0x01, 0x02, 0x01 };


/**
 * Returns an OUI sequence as a bitfield for authentication method
 * @s: string to decode. should be at least 4 byte long
 */
int wapi_auth_selector_to_bitfield(const u8 *s)
{
	if (os_memcmp(s, WAPI_AUTH_SUITE_CERTIFICATE, WAPI_SELECTOR_LEN) == 0)
		return WPA_KEY_MGMT_WAPI_CERT;

	if (os_memcmp(s, WAPI_AUTH_SUITE_PSK, WAPI_SELECTOR_LEN) == 0)
		return WPA_KEY_MGMT_WAPI_PSK;

	wpa_printf(MSG_ERROR, "WAPI: unknown authentication suite selector");
	return 0;
}

/**
 * returns an OUI sequence as a bitfield for cipher method
 * @s: string to decode. should be at least 4 byte long
 */
static int wapi_cipher_selector_to_bitfield(const u8 *s)
{
	if (os_memcmp(s, WAPI_CIPHER_SUITE_SMS4, WAPI_SELECTOR_LEN) == 0)
		return WPA_CIPHER_SMS4;
	wpa_printf(MSG_ERROR, "WAPI: unknown cipher suite selector");
	return 0;
}


/**
 * encodes an IE from AKM suite, unicast suite, multicast suite
 * @wapi_ie: buffer big enough to hold ie
 * @wapi_ie_len: length of buffer. holds length of generated ie after execution
 * @akm_suite: bitfield for supported akm suites
 * @unicast_cipher: bitfield for supported unicast ciphers
 * @multicast_cipher: bitfield for supported multicast ciphers
 */
int wapi_gen_ie(u8 *wapi_ie, size_t *wapi_ie_len, int akm_suite,
				int unicast_cipher, int multicast_cipher)
{
	u8 *pos = wapi_ie;

	if (*wapi_ie_len < WAPI_IE_MIN_LEN) {
		wpa_printf(MSG_ERROR, "WAPI: ie len too short (%lu)",
				(unsigned long) *wapi_ie_len);
		return -1;
	}

	*pos = WAPI_INFO_ELEM; /* 68 */
	++pos;

	++pos; /* skip length. we'll fill it in the end */

	WPA_PUT_LE16(pos, WAPI_VERSION); /* version */
	pos += 2;

	if ((akm_suite & WPA_KEY_MGMT_WAPI_PSK) &&
			(akm_suite & WPA_KEY_MGMT_WAPI_CERT)) {
		WPA_PUT_LE16(pos, 2);  /* akm count */
		pos += 2;
	} else if (akm_suite) {
		WPA_PUT_LE16(pos, 1);  /* akm count */
		pos += 2;
	}
	if (akm_suite & WPA_KEY_MGMT_WAPI_CERT) {
		os_memcpy(pos, WAPI_AUTH_SUITE_CERTIFICATE, WAPI_SELECTOR_LEN);
		pos += WAPI_SELECTOR_LEN;
	}
	if (akm_suite & WPA_KEY_MGMT_WAPI_PSK) {
		os_memcpy(pos, WAPI_AUTH_SUITE_PSK, WAPI_SELECTOR_LEN);
		pos += WAPI_SELECTOR_LEN;
	}

	WPA_PUT_LE16(pos, 1);  /* unicast cipher count */
	pos += 2;
	os_memcpy(pos, WAPI_CIPHER_SUITE_SMS4, WAPI_SELECTOR_LEN);
	pos += WAPI_SELECTOR_LEN;

	os_memcpy(pos, WAPI_CIPHER_SUITE_SMS4, WAPI_SELECTOR_LEN);
	pos += WAPI_SELECTOR_LEN;  /* multicast cipher count */

	WPA_PUT_LE16(pos, 0);  /* capabilities */
	pos += 2;

	WPA_PUT_LE16(pos, 0);  /* bkid count */
	pos += 2;

	*(wapi_ie + 1) = (pos-wapi_ie-2);
	*wapi_ie_len = pos-wapi_ie;

	return 0;
}


/**
 * decode byte array into struct wapi_ie
 * @wapi_ie_buf: buffer containing ie
 * @wapi_ie_len: length of buffer
 * @ie: space to decode ie to
 */
int wapi_parse_ie(const u8 *wapi_ie_buf, size_t wapi_ie_len,
		struct wapi_ie *ie)
{
	const u8 *pos, *end;
	int i, count;

	wpa_hexdump(MSG_DEBUG, "Got WAPI IE", wapi_ie_buf, wapi_ie_len);

	if (wapi_ie_len < WAPI_IE_MIN_LEN) {
		wpa_printf(MSG_ERROR, "WAPI: ie len too short (%lu)",
			(unsigned long) wapi_ie_len);
		return -1;
	}

	if (wapi_ie_len-2 != *(wapi_ie_buf+1)) {
		wpa_printf(MSG_ERROR, "WAPI: ie len mismatch (%lu)",
				(unsigned long) wapi_ie_len);
		return -1;
	}

	os_memset(ie, 0, sizeof(*ie));

	pos = wapi_ie_buf;
	end = pos + wapi_ie_len;

	ie->id = (u8) *pos++;
	ie->length = (u8) *pos++;

	ie->version = WPA_GET_LE16(pos);
	pos += 2;

	if (ie->version != WAPI_VERSION) {
		wpa_printf(MSG_ERROR, "WAPI: malformed ie or unknown version");
		return -1;
	}

	count = WPA_GET_LE16(pos);
	pos += 2;

	if (count == 0) {
		wpa_printf(MSG_ERROR, "WAPI: akm suite count = 0, ");
		return -1;
	}
	for (i = 0; i < count; ++i) {
		if (pos + WAPI_SELECTOR_LEN > end) {
			wpa_printf(MSG_ERROR,
					"WAPI: buffer is not long enough 1");
			return -1;
		}
		ie->akm_suite |= wapi_auth_selector_to_bitfield(pos);
		pos += WAPI_SELECTOR_LEN;
	}

	count = WPA_GET_LE16(pos);
	pos += 2;

	if (count == 0) {
		wpa_printf(MSG_ERROR, "WAPI: unicast cipher suite count = 0, ");
		return -1;
	}
	for (i = 0; i < count; ++i) {
		if (pos + WAPI_SELECTOR_LEN > end) {
			wpa_printf(MSG_ERROR,
					"WAPI: buffer is not long enough 2");
			return -1;
		}

		ie->unicast_suite |= wapi_cipher_selector_to_bitfield(pos);
		pos += WAPI_SELECTOR_LEN;

	}

	if (pos + WAPI_SELECTOR_LEN > end) {
		wpa_printf(MSG_ERROR, "WAPI: buffer is not long enough 3");
		return -1;
	}
	ie->multicast_suite |= wapi_cipher_selector_to_bitfield(pos);
	pos += WAPI_SELECTOR_LEN;

	ie->capabilities = WPA_GET_LE16(pos);
	pos += 2;

	return 0;
}

/**
 * convert subtype number to string
 */
static const char *wapi_subtype2str(wapi_msg_subtype subtype)
{
	switch (subtype) {
	case WAPI_SUBTYPE_PRE_AUTH_START:
		return "Pre-Authentication Start";
	case WAPI_SUBTYPE_ST_STAKEY_REQ:
		return "STAKey Request";
	case WAPI_SUBTYPE_AUTH_ACTIVACTION:
		return "Authentication Activation";
	case WAPI_SUBTYPE_ACCESS_AUTH_REQ:
		return "Access Authentication Request";
	case WAPI_SUBTYPE_ACCESS_AUT_RESP:
		return "Access Authentication Response";
	case WAPI_SUBTYPE_CERT_AUTH_REQ:
		return "Certificate Authentication Request";
	case WAPI_SUBTYPE_CERT_AUTH_RES:
		return "Certificate Authentication Response";
	case WAPI_SUBTYPE_UKEY_NEGO_REQ:
		return "Unicast Key Negotiation Request";
	case WAPI_SUBTYPE_UKEY_NEGO_RES:
		return "Unicast Key Negotiation Response";
	case WAPI_SUBTYPE_UKEY_NEGO_CONFIRM:
		return "Unicast Key Negotiation Confirmation";
	case WAPI_SUBTYPE_MKEY_STAKEY_ANNOUNCE:
		return "Multicast key/STAKey Announcement";
	case WAPI_SUBTYPE_MKEY_STAKEY_ANNOUNCE_RES:
		return "Multicast key/STAKey Announcement Response";
	}
	return "UNKNOWN MESSAGE SUBTYPE";
}




/**
 * WAPI Key derivation code (an hmac chain)
 *
 * @text: indicates the input text of the key derivation algorithm;
 * @text_len: indicates the length of the input text (in octet);
 * @key: indicates the input key of the key derivation algorithm;
 * @key_len: indicates the length of the input key (in octet);
 * @output: indicates the output of the key derivation algorithm;
 * @length: indicates the length of the output of
 *          the key derivation algorithm (in octet).
 */
void wapi_kd_hmac_sha256(const u8 *text, size_t text_len, const u8 *key,
			size_t key_len,	u8 *output, size_t length) {
	int i;
	u8 buffer[SHA256_DIGEST_SIZE];

	for (i = 0; length/SHA256_DIGEST_SIZE;
			i++, length -= SHA256_DIGEST_SIZE) {
		hmac_sha256(key, key_len, text, text_len,
				&output[i*SHA256_DIGEST_SIZE]);
		text = &output[i*SHA256_DIGEST_SIZE];
		text_len = SHA256_DIGEST_SIZE;
	}
	if (length > 0) {
		hmac_sha256(key, key_len, text, text_len, buffer);
		os_memcpy(&output[i*SHA256_DIGEST_SIZE], buffer, length);
	}

}

/**
 * Concatenate several buffers into one buffer
 * @elements: num of elements
 * @addr: an array containing the addresses of the buffers
 * @len: an array containing the lengths of the buffers
 * @buffer: a buffer to hold the result
 */
static void wapi_construct_buffer(int elements, const u8 *addr[],
		const size_t *len, u8 *buffer) {
	int i;
	u8 *pos = buffer;

	for (i = 0; i < elements; ++i) {
		os_memcpy(pos, addr[i], len[i]);
		pos += len[i];
	}
}


/**
 * Handles unicast key negotiation request.
 */
static int wapi_answer_ukey_req(struct wapi_sm *sm, struct wapi_msg *msg)
{
	/* 2*ETH_ALEN || challenge1 || challenge2 || expansion */
	u8 buffer[2*ETH_ALEN + 2*WAPI_CHALLENGE_LEN +
			WAPI_EXPANSION_BK_UCAST_LEN];

	const u8 *addr[4];
	size_t len[4];
	u8 *encoded_msg;
	size_t encoded_msg_len;
	struct wapi_msg reply;
	u8 bkid[WAPI_BKID_LEN];

	if (sm->state != WAPI_STATE_DONE)
		sm->ctx->set_state(sm->ctx->ctx, WPA_4WAY_HANDSHAKE);

	/* generate ASUE challenge */
	wpa_printf(MSG_DEBUG, "WAPI: generating challenge");
	if (sm->new_challenge) {
		if (os_get_random(sm->asue_challenge, WAPI_CHALLENGE_LEN)) {
			wpa_printf(MSG_ERROR,
					"WAPI: cannot generate random number");
			return -1;
		}
		sm->new_challenge = 0;
	}

	if (sm->cur_ssid && (sm->cur_ssid->key_mgmt & WPA_KEY_MGMT_WAPI_PSK)) {
		/* generate BK from passphrase if wasn't
		 * generated from certificate auth. */
		if (sm->cur_ssid->wapi_key_type == WAPI_AUTH_PSK_ASCII) {
			wpa_printf(MSG_DEBUG,
					"WAPI: using ASCII PSK '%s' to generate BK",
					sm->cur_ssid->wapi_psk);

			wapi_kd_hmac_sha256((const u8 *) WAPI_EXPANSION_PSK_BK,
					WAPI_EXPANSION_PSK_BK_LEN,
					(u8 *)sm->cur_ssid->wapi_psk,
					strlen((const char *)sm->cur_ssid->wapi_psk),
					sm->bk, WAPI_BK_LEN);
		} else if (sm->cur_ssid->wapi_key_type == WAPI_AUTH_PSK_HEX) {
			BIGNUM *parsed = NULL;
			char pskHex[WAPI_MAX_PSK_HEX_LEN];
			size_t pskHexLen;

			wpa_printf(MSG_DEBUG,
					"WAPI: using HEX PSK '%s' to generate BK",
					sm->cur_ssid->wapi_psk);

			parsed = BN_new();
			if (!parsed) {
				wpa_printf(MSG_ERROR, "WAPI: cannot create BIGNUM");
				return -1;
			}

			if (!BN_hex2bn(&parsed, sm->cur_ssid->wapi_psk)) {
				wpa_printf(MSG_ERROR, "WAPI: provided string does "
						"not contain a valid hex number");
				if (parsed)
					BN_free(parsed);
				return -1;
			}
			pskHexLen = BN_bn2bin(parsed, pskHex);

			wapi_kd_hmac_sha256((const u8 *) WAPI_EXPANSION_PSK_BK,
					WAPI_EXPANSION_PSK_BK_LEN,
					(u8 *)pskHex, pskHexLen,
					sm->bk, WAPI_BK_LEN);
		} else {
			wpa_printf(MSG_ERROR, "WAPI: invalid key type");
			return -1;
		}
		wpa_hexdump(MSG_DEBUG, "WAPI BK", sm->bk, WAPI_BK_LEN);
	} else
		wpa_printf(MSG_DEBUG, "WAPI: using certificate to generate BK");

	wapi_kd_hmac_sha256(msg->addid, 2*ETH_ALEN, sm->bk,
			WAPI_BK_LEN, bkid, WAPI_BKID_LEN);
	if (os_memcmp(msg->bkid, bkid, WAPI_BKID_LEN)) {
		wpa_printf(MSG_INFO,
				"WAPI: BKID doesn't match calculated BKID");
		return -1;
	} else
		wpa_printf(MSG_INFO, "WAPI: BKID matches");

	/* generate USK from BK, ASUE/AE challaneges,
	 * ADDID & expansion string */
	addr[0] = msg->addid;
	len[0] = 2*ETH_ALEN;
	addr[1] = msg->ae_challenge;
	len[1] = WAPI_CHALLENGE_LEN;
	addr[2] = sm->asue_challenge;
	len[2] = WAPI_CHALLENGE_LEN;
	addr[3] = (u8 *) WAPI_EXPANSION_BK_UCAST;
	len[3] = WAPI_EXPANSION_BK_UCAST_LEN;

	wapi_construct_buffer(4, addr, len, buffer);

	wapi_kd_hmac_sha256(buffer, sizeof(buffer), sm->bk, WAPI_BK_LEN,
			(u8 *)&sm->usk, sizeof(sm->usk));

	wpa_hexdump(MSG_DEBUG, "WAPI uek", sm->usk.uek, 16);
	wpa_hexdump(MSG_DEBUG, "WAPI uck", sm->usk.uck, 16);
	wpa_hexdump(MSG_DEBUG, "WAPI kck", sm->usk.kck, 16);
	wpa_hexdump(MSG_DEBUG, "WAPI kek", sm->usk.kek, 16);


	wpa_hexdump(MSG_DEBUG, "WAPI AE Challenge",
			msg->ae_challenge, WAPI_CHALLENGE_LEN);
	wpa_hexdump(MSG_DEBUG, "WAPI ASUE Challenge",
			sm->asue_challenge, WAPI_CHALLENGE_LEN);
	wpa_hexdump(MSG_DEBUG, "WAPI Next AE Challenge",
			sm->usk.challenge_seed, WAPI_CHALLENGE_LEN);

	/* create reply to AE */
	os_memset(&reply, 0, sizeof(reply));

	os_memcpy(reply.addid, msg->addid, ETH_ALEN*2);
	os_memcpy(reply.ae_challenge, msg->ae_challenge, WAPI_CHALLENGE_LEN);
	os_memcpy(reply.asue_challenge, sm->asue_challenge, WAPI_CHALLENGE_LEN);
	os_memcpy(&reply.flag, &(msg->flag), sizeof(reply.flag));
	os_memcpy(&reply.uskid, &(msg->uskid), sizeof(&reply.uskid));

	if (wapi_parse_ie(sm->ie_assoc, sm->ie_assoc_len, &reply.wapi_ie))
		return -1;
	/* os_memcpy(&reply.wapi_ie, &sm->ie, sizeof(reply.wapi_ie)); */

	os_memcpy(reply.kck, sm->usk.kck, WAPI_KCK_LEN);
	os_memcpy(reply.bkid, msg->bkid, WAPI_BKID_LEN);
	reply.header.subtype = WAPI_SUBTYPE_UKEY_NEGO_RES;
	os_memcpy(&reply.header.pkt_seq, &msg->header.pkt_seq,
		  sizeof(reply.header.pkt_seq));

	wpa_printf(MSG_DEBUG, "WAPI: Sending unicast negotiation response");
	if (wapi_encode_msg(sm, &reply, &encoded_msg, &encoded_msg_len, NULL)) {
		wpa_printf(MSG_ERROR, "WAPI: error encoding message");
		return -1;
	}
	wpa_hexdump(MSG_DEBUG, "WAPI unicast nego. dump",
			encoded_msg, encoded_msg_len);
	if (wapi_tx_wai(sm, sm->bssid, encoded_msg, encoded_msg_len) < 0) {
		wpa_printf(MSG_ERROR, "WAPI: error sending message");
		return -1;
	}
	os_free(encoded_msg);


	wpa_printf(MSG_DEBUG, "WAPI: setting key with keyidx %d",
			msg->uskid & WAPI_USKID_KEYID);

	if (sm->ctx->set_key(sm->ctx->ctx, WPA_ALG_WAPI,
			sm->bssid, msg->uskid & WAPI_USKID_KEYID, 1,
			NULL, 0, (u8 *)&sm->usk, WAPI_UEK_LEN + WAPI_UCK_LEN)) {
		wpa_printf(MSG_ERROR, "WAPI: error setting key");
		return -1;
	}

	sm->state = WAPI_STATE_USKNEGOTIATING;
	return 0;
}


/**
 * Handles unicast key negotiation confirmation
 */
int wapi_process_ukey_confirm(struct wapi_sm *sm, struct wapi_msg *msg,
		const u8 mac[WAPI_MAC_LEN]) {
	if (os_memcmp(sm->asue_challenge, msg->asue_challenge,
			WAPI_CHALLENGE_LEN)) {
		wpa_printf(MSG_ERROR,
				"WAPI: asue challenge doesn't match challenge"
				" from Unicast Key Negotiation Response message");
		wpa_hexdump(MSG_DEBUG, "WAPI current  asue challange",
				msg->asue_challenge, WAPI_CHALLENGE_LEN);
		wpa_hexdump(MSG_DEBUG, "WAPI previous asue challange",
				sm->asue_challenge, WAPI_CHALLENGE_LEN);
		return -1;
	}
	if (os_memcmp(msg->msg_auth_code, mac, WAPI_MAC_LEN)) {
		wpa_printf(MSG_ERROR, "WAPI: calculated MAC doesn't match "
				"MAC from message");
		wpa_hexdump(MSG_DEBUG, "WAPI MAC from msg  ",
				msg->msg_auth_code, WAPI_MAC_LEN);
		wpa_hexdump(MSG_DEBUG, "WAPI calculated MAC",
				mac, WAPI_MAC_LEN);

		return -1;
	}

	sm->new_challenge = 1;
	sm->state = WAPI_STATE_USKDONE;

	return 0;
}

/**
 * wapi_notify_assoc - Notify WAPI state machine about association
 * @sm: Pointer to WAPI state machine data from wpa_sm_init()
 * @bssid: The BSSID of the new association
 *
 * This function is called to let WAPI state machine know that the connection
 * was established.
 */
void wapi_notify_assoc(struct wapi_sm *sm, const u8 *bssid)
{
	if (sm == NULL)
		return;

	os_memcpy(sm->bssid, bssid, ETH_ALEN);
}


/**
 * Saves association IE in WAPI SM
 * @sm: wapi sm
 * @ie: ie buffer
 * @len: ie len
 */
int wapi_set_assoc_ie(struct wapi_sm *sm, const u8 *ie, size_t len)
{
	if (!sm) {
		wpa_printf(MSG_ERROR, "WAPI: sm is null");
		return -1;
	}

	os_free(sm->ie_assoc);
	sm->ie_assoc = NULL;
	sm->ie_assoc_len = 0;

	if (ie && len) {
		sm->ie_assoc = os_malloc(len);
		if (!sm->ie_assoc) {
			wpa_printf(MSG_ERROR, "WAPI: error allocating memory");
			return -1;
		}
		sm->ie_assoc_len = len;
		os_memcpy(sm->ie_assoc, ie, len);
	}

	return 0;
}

/**
 * Saves IE from associated AP
 * @sm: wapi sm
 * @ie: ie buffer
 * @len: ie len
 */
int wapi_set_ap_ie(struct wapi_sm *sm, const u8 *ie, size_t len)
{
	if (!sm) {
		wpa_printf(MSG_ERROR, "WAPI: sm is nulll");
		return -1;
	}

	os_free(sm->ie_ap);
	sm->ie_ap = NULL;
	sm->ie_ap_len = 0;

	if (ie && len) {
		sm->ie_ap = os_malloc(len);
		if (!sm->ie_ap) {
			wpa_printf(MSG_ERROR, "WAPI: error allocating memory");
			return -1;
		}
		sm->ie_ap_len = len;
		os_memcpy(sm->ie_ap, ie, len);
	}

	return 0;
}


/**
 * Calculate WAPI mac on a WAI message that includes a header.
 * Automatically skips the WAI header and ignores the last WAPI_MAC_LEN bytes
 * @kck: key
 * @msg: WAI message
 * @len: mesage length
 * @mac: buffer for storing the MAC
 */
void wapi_calc_mac(const u8 *kck, const u8 *msg, size_t len,
		u8 mac[WAPI_MAC_LEN]) {
	u8 buffer[SHA256_DIGEST_SIZE];
	hmac_sha256(kck, WAPI_KCK_LEN, msg+sizeof(struct wapi_wai_hdr),
			len-sizeof(struct wapi_wai_hdr)-WAPI_MAC_LEN, buffer);
	os_memcpy(mac, buffer, WAPI_MAC_LEN);
}

/**
 * Compares two hex arrays of the same size.
 * @first: first array
 * @second: second array
 * @len: length of arrays
 * Returns: -1 if first is bigger, 0 if equal, 1 if second is bigger
 */
int wapi_cmp_hex(const u8 *first, const u8 *second, size_t len)
{
	size_t i;

	if (!first || !second) {
		wpa_printf(MSG_INFO, "WAPI: comparison received null");
		return 0;
	}

	for (i = 0; i < len; ++i) {
		if (first[i] > second[i])
			return -1;
		if (first[i] < second[i])
			return 1;
	}
	return 0;
}


/**
 * handles multicast key announcement message
 */
int wapi_answer_mkey_announce(struct wapi_sm *sm, struct wapi_msg *msg,
		const u8 mac[WAPI_MAC_LEN])
{
	u8 nmk[WAPI_NMK_LEN];
	struct wapi_msg reply;
	u8 *encoded_msg;
	size_t encoded_msg_len;

	if (msg->flag & WAPI_FLAG_STAKEY_NEGO) {
		wpa_printf(MSG_INFO, "WAPI: packet discarded. no support for "
				"STAKey announcement");
		return -1;
	}

	if (os_memcmp(msg->msg_auth_code, mac, WAPI_MAC_LEN)) {
		wpa_printf(MSG_INFO, "WAPI: packet discarded. "
				"calculated MAC doesn't match MAC from message");
		return -1;
	}

	if (wapi_cmp_hex(sm->prev_key_announcement, msg->key_announcment_id,
			WAPI_KEY_ANNOUNCMENT_ID_LEN) >= 0) {
		os_memcpy(sm->prev_key_announcement, msg->key_announcment_id,
						WAPI_KEY_ANNOUNCMENT_ID_LEN);
	} else {
		wpa_printf(MSG_INFO, "WAPI: packet discarded. "
				"key announcement id is not monotonous");
		return -1;
	}

	/* decode nmk using sms4 and then use it to generate MSK + MIC using
	 * expansion string	 */
	sms4_decrypt(sm->usk.kek, msg->key_announcment_id,
			msg->key_data.content, nmk);
	wpa_printf(MSG_DEBUG, "WAPI: decoding nmk");
	wpa_hexdump(MSG_DEBUG, "WAPI IV",
			msg->key_announcment_id, WAPI_KEY_ANNOUNCMENT_ID_LEN);
	wpa_hexdump(MSG_DEBUG, "WAPI encrypted key",
			msg->key_data.content, msg->key_data.len);
	wpa_hexdump(MSG_DEBUG, "WAPI nmk", nmk, WAPI_NMK_LEN);

	wapi_kd_hmac_sha256((u8 *)WAPI_EXPANSION_NMK, WAPI_EXPANSION_NMK_LEN,
			nmk, WAPI_NMK_LEN, (u8 *)&sm->mkey,
			WAPI_MSK_LEN+WAPI_MIC_LEN);

	os_memset(&reply, 0, sizeof(reply));
	reply.header.subtype = WAPI_SUBTYPE_MKEY_STAKEY_ANNOUNCE_RES;
	reply.flag = msg->flag;
	reply.mskid_stakid = msg->mskid_stakid;
	reply.uskid = msg->uskid;
	os_memcpy(&reply.header.pkt_seq, &msg->header.pkt_seq,
		  sizeof(reply.header.pkt_seq));
	os_memcpy(&reply.addid, &msg->addid, ETH_ALEN*2);
	os_memcpy(&reply.key_announcment_id, &msg->key_announcment_id,
			WAPI_KEY_ANNOUNCMENT_ID_LEN);
	os_memcpy(reply.kck, sm->usk.kck, WAPI_KCK_LEN);

	wpa_printf(MSG_DEBUG,
			"WAPI: Sending multicast key announcement response");
	if (wapi_encode_msg(sm, &reply, &encoded_msg, &encoded_msg_len, NULL)) {
		wpa_printf(MSG_ERROR, "WAPI: packet encoding error");
		return -1;
	}
	if (wapi_tx_wai(sm, sm->bssid, encoded_msg, encoded_msg_len) < 0) {
		wpa_printf(MSG_ERROR, "WAPI: packet tx error");
		return -1;
	}
	os_free(encoded_msg);

	if (sm->ctx->set_key(sm->ctx->ctx, WPA_ALG_WAPI,
			(u8 *) "\xff\xff\xff\xff\xff\xff",
			msg->mskid_stakid & WAPI_MSKID_KEYID, 1,
			NULL, 0, (u8 *)&sm->mkey, WAPI_MSK_LEN+WAPI_MIC_LEN)) {
		wpa_printf(MSG_ERROR, "WAPI: error setting key");
		return -1;
	}
	return 0;
}

int wapi_send_split_msg(struct wapi_sm *sm, const u8 *dest,
			       const u8 *msg, size_t msg_len) {
	u8 split[2000];
	struct wapi_wai_hdr *hdr;
	int msg_count = 0;
	const u8 *pos;
	size_t len_to_go;
	size_t cur_len;

	pos = msg + sizeof(struct wapi_wai_hdr);
	hdr = (struct wapi_wai_hdr *) split;
	len_to_go = msg_len - sizeof(struct wapi_wai_hdr);

	wpa_printf(MSG_DEBUG, "WAPI: splitting messages before send");

	while (len_to_go > 0) {
		os_memcpy(split, msg, sizeof(struct wapi_wai_hdr));
		cur_len = MIN(WAPI_MTU, len_to_go);
		os_memcpy(split+sizeof(struct wapi_wai_hdr), pos, cur_len);
		pos += cur_len;
		len_to_go -= cur_len;

		if (len_to_go > 0)
			hdr->more_frag |= WAPI_MORE_FRAG;
		hdr->frag_seq = msg_count++;

		WPA_PUT_BE16(hdr->len, cur_len+sizeof(struct wapi_wai_hdr));

		wpa_printf(MSG_DEBUG, "WAPI: sending msg #%d with length %d",
				hdr->frag_seq, cur_len);

		if (wapi_tx_wai(sm, dest, split,
				cur_len + sizeof(struct wapi_wai_hdr)) < 0) {
			wpa_printf(MSG_ERROR, "WAPI: packet tx error ");
			return -1;
		}
	}

	return 0;
}



int wapi_answer_auth_activation(struct wapi_sm *sm, struct wapi_msg *msg)
{
	struct wapi_msg reply;
	EC_GROUP *group = NULL;
	int retval = -1;
	u8 *encoded_msg = NULL;
	size_t encoded_msg_len;
	u8 identity_ap[500], identity_sta[500];
	X509 *sta = NULL;
	u8 *p;

	os_memset(&reply, 0, sizeof(reply));
	reply.header.subtype = WAPI_SUBTYPE_ACCESS_AUTH_REQ;
	reply.flag |= WAPI_FLAG_CERT_AUTH_REQ;
	os_memcpy(reply.auth_id, msg->auth_id, WAPI_AUTH_ID_LEN);
	if (os_get_random(reply.asue_challenge, WAPI_CHALLENGE_LEN)) {
		wpa_printf(MSG_ERROR, "WAPI: error generating random number");
		goto end;
	}
	if (sm->new_ecdh) {
		wpa_printf(MSG_DEBUG, "WAPI: generating new ecdh key");
		group = wapi_ec_group();
		if (!group || !EC_KEY_set_group(sm->ecdh_key, group) ||
				!EC_KEY_generate_key(sm->ecdh_key)) {
			wpa_printf(MSG_ERROR,
					"WAPI: error setting ecdh parameters");
			goto end;
		}
		sm->new_ecdh = 0;
	}
	p = msg->ae_cert.value;

	if (sm->ae_cert)
		X509_free(sm->ae_cert);

	sm->ae_cert = d2i_X509(NULL,
			(const unsigned char **)&p, msg->ae_cert.len);
	if (!sm->ae_cert) {
		wpa_printf(MSG_ERROR, "WAPI: error decoding ae certificate");
		goto end;
	}
	if (wapi_encode_pubkey(sm->ecdh_key, &reply.asue_key_data)) {
		wpa_printf(MSG_ERROR, "WAPI: error encoding pubkey");
		goto end;
	}

	reply.id_ae.len = wapi_construct_identity(sm->ae_cert, identity_ap);
	reply.id_ae.type = 1;
	reply.id_ae.value = identity_ap;
	if (reply.id_ae.len < 0)
		goto end;
	wpa_hexdump(MSG_DEBUG, "WAPI identity_ap",
			identity_ap, reply.id_ae.len);

	reply.asue_cert.len = sm->asue_cert_len;
	reply.asue_cert.type = 1;
	reply.asue_cert.value = sm->asue_cert;

	reply.ecdh_params.len = 11;
	reply.ecdh_params.type = 1;
	reply.ecdh_params.value = (u8 *) WAPI_ECDH_PARAMS;

	p = sm->asue_cert;
	sta = d2i_X509(NULL, (const unsigned char **)&p, sm->asue_cert_len);
	if (!sta) {
		wpa_printf(MSG_ERROR, "WAPI: error parsing sta cert");
		goto end;
	}

	reply.signature.identity.type = 1;
	reply.signature.identity.len =
			wapi_construct_identity(sta, identity_sta);
	reply.signature.identity.value = identity_sta;
	if (reply.signature.identity.len < 0)
		goto end;
	wpa_hexdump(MSG_DEBUG, "WAPI identity_sta",
			identity_sta, reply.signature.identity.len);

	reply.signature.id = 1;
	reply.signature.len = 72+reply.signature.identity.len;
	os_memcpy(reply.signature.signature_alg, WAPI_SIGNATURE_ALG,
			WAPI_SIGNATURE_ALG_LEN);
	reply.signature.signature.len = 48;

	wpa_printf(MSG_DEBUG, "WAPI: encoding access authentication request");
	if (wapi_encode_msg(sm, &reply, &encoded_msg, &encoded_msg_len,
			sm->asue_priv_key)) {
		wpa_printf(MSG_ERROR, "WAPI: packet encoding error ");
		goto end;
	}

	wpa_printf(MSG_DEBUG, "WAPI: Sending access authentication request");

	if (wapi_send_split_msg(sm, sm->bssid, encoded_msg, encoded_msg_len)) {
		wpa_printf(MSG_ERROR, "WAPI: packet split tx error");
		goto end;
	}

	retval = 0;
end:
	if (group)
		EC_GROUP_free(group);
	os_free(encoded_msg);

	if (sta)
		X509_free(sta);

	return retval;
}


/* returns 0 if succesful, 1 if unsuccesful, 2 on error */
int validate_mcvr(struct wapi_sm *sm, struct wapi_msg *msg)
{
	u8 cert[5000];
	u8 *p;
	int retval;

	if (!sm->root_cert) {
		wpa_printf(MSG_ERROR, "WAPI: no root certificate available");
		return 2;
	}
	switch (wapi_ecdsa_verify_frame(sm->root_cert,
			msg->auth_result.raw_mcvr,
			msg->auth_result.raw_mcvr_len,
			msg->server_signature_asue.ecdsa)) {
	case 0:
		wpa_printf(MSG_DEBUG,
				"WAPI: successfully verified mcvr signature");
		break;
	case 1:
		wpa_printf(MSG_INFO, "WAPI: couldn't verify mcvr signature");
		return 1;
	case 2:
		wpa_printf(MSG_ERROR, "WAPI: error verifying mcvr signature");
		return 2;
	}

	if (os_memcmp(msg->ae_challenge, msg->auth_result.nonce1,
			WAPI_CHALLENGE_LEN)) {
		wpa_printf(MSG_INFO, "WAPI: nonce1 doesn't match ae_challenge");
		wpa_hexdump(MSG_DEBUG, "WAPI nonce",
				msg->auth_result.nonce1, WAPI_CHALLENGE_LEN);
		wpa_hexdump(MSG_DEBUG, "WAPI ae_challenge",
				msg->ae_challenge, WAPI_CHALLENGE_LEN);
		return 1;
	}

	if (os_memcmp(msg->asue_challenge, msg->auth_result.nonce2,
			WAPI_CHALLENGE_LEN)) {
		wpa_printf(MSG_INFO,
				"WAPI: nonce2 doesn't match asue_challenge");
		wpa_hexdump(MSG_DEBUG, "WAPI nonce2",
				msg->auth_result.nonce2, WAPI_CHALLENGE_LEN);
		wpa_hexdump(MSG_DEBUG, "WAPI asue_challenge",
				msg->asue_challenge, WAPI_CHALLENGE_LEN);
		return 1;
	}
	if (msg->auth_result.ver_result1 != WAPI_ACCESS_RESULT_SUCCESS) {
		wpa_printf(MSG_INFO, "WAPI: failed on verification result 1");
		return 1;
	}
	if (msg->auth_result.ver_result2 != WAPI_ACCESS_RESULT_SUCCESS) {
		wpa_printf(MSG_INFO, "WAPI: failed on verification result 2");
		return 1;
	}
	if (os_memcmp(sm->asue_cert, msg->auth_result.cert1.value,
			sm->asue_cert_len)) {
		wpa_printf(MSG_INFO, "WAPI: certificate1 doesn't match");
		wpa_hexdump(MSG_DEBUG, "WAPI asue_cert",
				sm->asue_cert, sm->asue_cert_len);
		wpa_hexdump(MSG_DEBUG, "WAPI msg_cert",
				msg->auth_result.cert1.value,
				msg->auth_result.cert1.len);
		return 1;
	}
	p = cert;
	retval = i2d_X509(sm->ae_cert, &p);
	if (retval < 0) {
		wpa_printf(MSG_ERROR, "WAPI: error decoding certificate");
		return 2;
	}
	if (os_memcmp(cert, msg->auth_result.cert2.value, retval)) {
		wpa_printf(MSG_INFO, "WAPI: certificate2 doesn't match");
		wpa_hexdump(MSG_DEBUG, "WAPI ae_cert", cert, retval);
		wpa_hexdump(MSG_DEBUG, "WAPI msg_cert",
				msg->auth_result.cert2.value,
				msg->auth_result.cert2.len);
		return 1;
	}
	return 0;
}

int wapi_process_aa_response(struct wapi_sm *sm, struct wapi_msg *msg,
				const u8 *raw_msg)
{
	EC_POINT *bk = NULL;
	const EC_GROUP *group = NULL;
	BIGNUM *bk_x = NULL;
	const BIGNUM *priv_key = NULL;
	u8 bk_x_buf[WAPI_EC_PRIV_LEN];
	int retval = -1;
	const u8 *addr[3];
	size_t len[3];
	u8 expansion_buf[WAPI_EXPANSION_EC_BK_LEN+2*WAPI_CHALLENGE_LEN];

	switch (wapi_ecdsa_verify_frame(sm->ae_cert,
			raw_msg+sizeof(struct wapi_wai_hdr),
			msg->signed_content_length, msg->signature.ecdsa)) {
	case 0:
		wpa_printf(MSG_DEBUG,
				"WAPI: successfully verified message from ae");
		break;
	case 1:
		wpa_printf(MSG_INFO, "WAPI: couldn't verify message from ae");
		goto end;
		break;
	case 2:
		wpa_printf(MSG_ERROR, "WAPI: error verifying message from ae");
		goto end;
		break;
	}

	if (msg->flag & WAPI_FLAG_OPTIONAL) {
		wpa_printf(MSG_DEBUG, "WAPI: validating mcvr");

		switch (validate_mcvr(sm, msg)) {
		case 0:
			wpa_printf(MSG_DEBUG,
					"WAPI: successfully validated mcvr");
			break;
		case 1:
			wpa_printf(MSG_INFO, "WAPI: mcvr wasn't validated. "
					"deauthenticating");
			/* 1 == reason unspecified */
			sm->ctx->deauthenticate(sm->ctx->ctx, 1);
			goto end;
		case 2:
			wpa_printf(MSG_ERROR, "WAPI: error validating mcvr");
			goto end;
		}
	} else
		wpa_printf(MSG_INFO, "WAPI: mcvr is not present");

	if (msg->access_result == WAPI_ACCESS_RESULT_SUCCESS)
		wpa_printf(MSG_DEBUG,
				"WAPI: received successful access result");
	else {
		wpa_printf(MSG_INFO, "WAPI: received UNSUCCESSFUL "
				"access result. deauthenticating");
		/* 1 == reason unspecified */
		sm->ctx->deauthenticate(sm->ctx->ctx, 1);
		goto end;
	}

	/* compute bk */
	wpa_printf(MSG_DEBUG, "WAPI: compute bk");
	group = EC_KEY_get0_group(sm->ecdh_key);
	bk = EC_POINT_new(group);
	bk_x = BN_new();
	if (!bk || !bk_x) {
		wpa_printf(MSG_ERROR, "WAPI: error creating openssl elements");
		goto end;
	}

	if (!EC_POINT_oct2point(group, bk,
			msg->ae_key_data.content, msg->ae_key_data.len, NULL)) {
		wpa_printf(MSG_ERROR, "WAPI: error converting oct2point");
		goto end;
	}

	priv_key = EC_KEY_get0_private_key(sm->ecdh_key);
	wpa_printf(MSG_DEBUG, "WAPI: temporary ecdh private key %s",
			BN_bn2hex(priv_key));
	if (!EC_POINT_mul(group, bk, NULL, bk, priv_key, NULL)) {
		wpa_printf(MSG_ERROR, "WAPI: error creating mutual secret");
		goto end;
	}

	if (!EC_POINT_get_affine_coordinates_GFp(group, bk, bk_x, NULL, NULL)) {
		wpa_printf(MSG_ERROR, "WAPI: error retreiving x coordinate");
		goto end;
	}
	if (BN_bn2bin(bk_x, bk_x_buf) != WAPI_EC_PRIV_LEN) {
		wpa_printf(MSG_ERROR, "WAPI: error in number of bytes written");
		goto end;
	}
	wpa_hexdump(MSG_DEBUG, "WAPI shared secret x",
			bk_x_buf, WAPI_EC_PRIV_LEN);

	addr[0] = msg->ae_challenge;
	len[0]  = WAPI_CHALLENGE_LEN;
	addr[1] = msg->asue_challenge;
	len[1]  = WAPI_CHALLENGE_LEN;
	addr[2] = (u8 *) WAPI_EXPANSION_EC_BK;
	len[2]  = WAPI_EXPANSION_EC_BK_LEN;

	wapi_construct_buffer(3, addr, len, expansion_buf);
	wpa_hexdump(MSG_DEBUG, "WAPI expansion for bk",
			expansion_buf,
			WAPI_EXPANSION_EC_BK_LEN+2*WAPI_CHALLENGE_LEN);

	wapi_kd_hmac_sha256(expansion_buf,
			WAPI_EXPANSION_EC_BK_LEN+2*WAPI_CHALLENGE_LEN,
			bk_x_buf, WAPI_EC_PRIV_LEN, sm->bk, WAPI_BK_LEN);

	wpa_hexdump(MSG_DEBUG, "WAPI BK", sm->bk, WAPI_BK_LEN);

	retval = 0;
end:
	if (bk)
		EC_POINT_free(bk);
	if (bk_x)
		BN_free(bk_x);

	return retval;
}


static int wapi_load_cert_from_keystore(char *uri, u8 **cert) {
	int len = -1;
	if (uri != NULL && strncmp("keystore://", uri, 11) == 0) {
		*cert = os_malloc(KEYSTORE_MESSAGE_SIZE);

		if (*cert != NULL) {
			len = keystore_get(&uri[11], strlen(&uri[11]), (char*)(*cert));
		}
	}
	return len;
}


int wapi_retrieve_cert(struct wapi_sm *sm) {
	int ret_val = -1;

	u8 *as_cert = NULL;
	u8 *user_cert = NULL;
	u8 *user_key = NULL;
	int as_cert_len = -1;
	int user_cert_len = -1;
	int user_key_len = -1;


	wpa_printf(MSG_DEBUG, "WAPI: parsing content from repository");

	if (sm->asue_cert_len) {
		wpa_printf(MSG_DEBUG, "WAPI: already holding certificate");
		ret_val = 0;
		goto end;
	}

	if ((!sm->cur_ssid->as_cert_uri) ||
            (!sm->cur_ssid->user_cert_uri) ||
            (!sm->cur_ssid->user_key_uri)) {
		wpa_printf(MSG_ERROR,
			"WAPI: no wapi certificate/private key filenames were set");
		goto end;
	}

	as_cert_len = wapi_load_cert_from_keystore(sm->cur_ssid->as_cert_uri, &as_cert);
	if (as_cert_len < 0) {
		wpa_printf(MSG_ERROR, "WAPI: error opening as certificate");
		goto end;
	}

	user_cert_len = wapi_load_cert_from_keystore(sm->cur_ssid->user_cert_uri, &user_cert);
	if (user_cert_len < 0) {
		wpa_printf(MSG_ERROR, "WAPI: error opening user certificate");
		goto end;
	}

	user_key_len = wapi_load_cert_from_keystore(sm->cur_ssid->user_key_uri, &user_key);
	if (user_key_len < 0) {
		wpa_printf(MSG_ERROR, "WAPI: error opening user key");
		goto end;
	}

	if (wapi_ec_extract_pkey_from_PEM((char *)user_key, user_key_len,
			sm->asue_priv_key)) {
		wpa_printf(MSG_ERROR, "WAPI: error extracting private key");
		goto end;
	}

	if (wapi_ec_X509_PEM_to_DER((char *)user_cert, user_cert_len,
			&(sm->asue_cert_len), sm->asue_cert)) {
		wpa_printf(MSG_ERROR, "WAPI: error decoding user certificate");
		goto end;
	}

	if (sm->root_cert)
		X509_free(sm->root_cert);
	sm->root_cert = wapi_X509_from_PEM((char *)as_cert, as_cert_len);
	if (!sm->root_cert) {
		wpa_printf(MSG_ERROR, "WAPI: error decoding root certificate");
		goto end;
	}

	ret_val = 0;
end:
	if (as_cert)
		os_free(as_cert);
	if (user_cert)
		os_free(user_cert);
	if (user_key)
		os_free(user_key);


	return ret_val;
}


u8 *wapi_encode_signature(u8 *pos, struct wapi_signature *sign)
{
	*pos++ = sign->id; /* id */
	WPA_PUT_BE16(pos, sign->len); /* len */
	pos += 2;

	WPA_PUT_BE16(pos, sign->identity.type); /* identity id */
	pos += 2;

	WPA_PUT_BE16(pos, sign->identity.len); /* identity len */
	pos += 2;

	os_memcpy(pos, sign->identity.value, sign->identity.len);
	pos += sign->identity.len;

	os_memcpy(pos, sign->signature_alg, WAPI_SIGNATURE_ALG_LEN);
	pos += WAPI_SIGNATURE_ALG_LEN;

	WPA_PUT_BE16(pos, WAPI_EC_ECDSA_LEN);
	pos += 2;

	os_memcpy(pos, sign->ecdsa, WAPI_EC_ECDSA_LEN);
	pos += WAPI_EC_ECDSA_LEN;

	return pos;
}


/**
 * Entry point of the WAPI SM. This function is called by wpa_supplicant_rx_wai
 * which is registered as the callback function using l2_packet_init().
 * The function is invoked each time a new WAI message is received.
 * If a message is received out of context, is invalid, contains wrong MAC,
 * etc, it will be discarded.
 * @sm: wapi sm
 * @src_addr: source address
 * @buf: buffer holding the message
 * @len: length of buffer
 */
void wapi_rx_wai(struct wapi_sm *sm, const u8 *src_addr,
				const u8 *buf, size_t len)
{
	struct wapi_msg msg; /* can be used only while buf is in scope */
			     /* because the struct has references to it */

	u8 mac[WAPI_MAC_LEN];
	const struct wapi_wai_hdr *hdr = (const struct wapi_wai_hdr *) buf;

	os_memset(&msg, 0, sizeof(msg));
	wpa_printf(MSG_DEBUG, "WAPI: received WAI message: %s",
			wapi_subtype2str(hdr->subtype));

	if ((hdr->more_frag & WAPI_MORE_FRAG) && !hdr->frag_seq) {
		wpa_printf(MSG_DEBUG, "WAPI: receiving first "
				"fragment with length %d", len);

		/* cleaning previously used buffer (of previous msgs) */
		os_free(sm->trunc_msg);
		sm->trunc_msg = os_malloc(len);
		if (!sm->trunc_msg) {
			wpa_printf(MSG_ERROR, "WAPI: failed on malloc");
			return;
		}
		os_memcpy(sm->trunc_msg, buf, len);
		sm->trunc_msg_len = len;
		return;
	}
	if (hdr->frag_seq) {
		wpa_printf(MSG_DEBUG,
				"WAPI: receiving fragment #%d with length %d",
				hdr->frag_seq+1, len);
		if (!sm->trunc_msg) {
			wpa_printf(MSG_DEBUG, "WAPI: wrong order of "
					"fragments (where's first fragment?)");
			return;
		}
		sm->trunc_msg = os_realloc(sm->trunc_msg,
				sm->trunc_msg_len + len -
				sizeof(struct wapi_wai_hdr));
		if (!sm->trunc_msg) {
			wpa_printf(MSG_ERROR, "WAPI: failed on realloc");
			return;
		}
		os_memcpy(sm->trunc_msg+sm->trunc_msg_len,
				buf+sizeof(struct wapi_wai_hdr),
				len-sizeof(struct wapi_wai_hdr));
		sm->trunc_msg_len += len-sizeof(struct wapi_wai_hdr);

		if (hdr->more_frag & WAPI_MORE_FRAG)
			return;
		wpa_printf(MSG_DEBUG, "WAPI: received last fragment of msg");
		wpa_printf(MSG_DEBUG, "WAPI: total length is %d",
				sm->trunc_msg_len);

		buf = sm->trunc_msg;
		len = sm->trunc_msg_len;
	}

	/* NOTICE: msg can be used only while buf is in scope!!! */
	if (wapi_decode_msg(&msg, buf, len)) {
		wpa_printf(MSG_ERROR, "WAPI: fail to decode WAPI msg");
		return;
	}

	switch (msg.header.subtype) {
	case WAPI_SUBTYPE_AUTH_ACTIVACTION:
		/* save message a second time for when reentering sm */

		if (!sm->fetch_cert) {
			/* first time - fetch cert from repository */
			wpa_printf(MSG_DEBUG,
					"WAPI: fetching cert from repository");
			os_free(sm->reentrant_raw);
			sm->reentrant_raw = os_malloc(len);
			if (!sm->reentrant_raw) {
				wpa_printf(MSG_ERROR, "WAPI: failed on malloc");
				return;
			}
			os_memcpy(sm->reentrant_raw, buf, len);
			sm->reentrant_raw_len = len;

			if (wapi_decode_msg(&(sm->reentrant_msg),
					sm->reentrant_raw,
					sm->reentrant_raw_len)) {
				wpa_printf(MSG_ERROR,
						"WAPI: fail to decode WAPI msg");
				return;
			} else
				wpa_printf(MSG_DEBUG, "WAPI: successfully "
						"decoded reentrant msg");
			sm->fetch_cert = 1;
			wapi_retrieve_cert(sm);
		} else if (sm->asue_cert_len) {
			wpa_printf(MSG_DEBUG,
					"WAPI: calling "
					"wapi_answer_auth_activation() regular mode");
			if (wapi_answer_auth_activation(sm, &msg)) {
				wpa_printf(MSG_ERROR, "WAPI: error "
						"wapi_answer_auth_activation");
				return;
			}
		}

		break;
	case WAPI_SUBTYPE_ACCESS_AUT_RESP:
		if (wapi_process_aa_response(sm, &msg, buf)) {
			wpa_printf(MSG_DEBUG,
					"WAPI: wapi_process_aa_response()");
			return;
		}
		break;

	case WAPI_SUBTYPE_UKEY_NEGO_REQ:
		if (wapi_answer_ukey_req(sm, &msg)) {
			wpa_printf(MSG_DEBUG, "WAPI: wapi_answer_ukey_req()");
			return;
		}
		break;


	case WAPI_SUBTYPE_UKEY_NEGO_CONFIRM:
		wapi_calc_mac(sm->usk.kck, buf, len, mac);
		if (wapi_process_ukey_confirm(sm, &msg, mac)) {
			wpa_printf(MSG_DEBUG,
					"WAPI: wapi_process_ukey_confirm()");
			return;
		}
		break;

	case WAPI_SUBTYPE_MKEY_STAKEY_ANNOUNCE:
		wapi_calc_mac(sm->usk.kck, buf, len, mac);

		if (sm->state != WAPI_STATE_DONE)
			sm->ctx->set_state(sm->ctx->ctx, WPA_GROUP_HANDSHAKE);

		if (wapi_answer_mkey_announce(sm, &msg, mac)) {
			wpa_printf(MSG_DEBUG,
					"WAPI: wapi_answer_mkey_announce()");
			return;
		}

		if (sm->state != WAPI_STATE_DONE) {

			sm->ctx->cancel_auth_timeout(sm->ctx->ctx);
			sm->ctx->cancel_scan(sm->ctx->ctx);
			if (sm->ctx->set_port)
				sm->ctx->set_port(sm->ctx->ctx, 1);
			sm->state = WAPI_STATE_DONE;

			sm->ctx->set_state(sm->ctx->ctx, WPA_COMPLETED);
		}

		break;
	default:
		wpa_printf(MSG_INFO, "WAPI: packet discarded. "
				"not supported subtype %d",
				msg.header.subtype);
		return;
	}
}


/* macros to read/write an arbitrary field from/to the buffer.
   assumes the following variables in scope:
   'params' - struct to hold parameters
   'pos'    = pointer to current position in buffer
   'len'    - remaining space in buffer

   always works in BIG-ENDIAN mode.
   will update params, pos and len.
   will return -1 on error.
*/


#define READ_BUFFER(dest, len_to_read)					\
	do {if (len_to_read > len) {					\
		wpa_printf(MSG_ERROR, "WAPI: error parsing field " #dest);\
		return -1; }						\
		if (len_to_read == 2)					\
			*((u16 *)(dest)) = WPA_GET_BE16(pos);		\
		else if (len_to_read == 4)				\
			*((u32 *)(dest)) = WPA_GET_BE32(pos);		\
		else							\
			os_memcpy((u8 *)(dest), pos, (len_to_read));	\
		pos += (len_to_read);					\
		len -= (len_to_read); } while (0)

#define READ_SINGLE_FIELD(field)			\
READ_BUFFER(&(params->field), sizeof(params->field))

#define WRITE_BUFFER(src, len_to_write)				\
	do {if (len_to_write > len) {					\
		wpa_printf(MSG_ERROR, "WAPI: error encoding field " #src);\
		return -1; }						\
		if (len_to_write == 2)					\
			WPA_PUT_BE16(pos, *((u16 *)(src)));		\
		else if (len_to_write == 4)				\
			WPA_PUT_BE32(pos, *((u32 *)(src)));		\
		else							\
			os_memcpy(pos, (const u8 *)(src), (len_to_write));\
		pos += (len_to_write);					\
		len -= (len_to_write); } while (0)

#define WRITE_SINGLE_FIELD(field)			\
WRITE_BUFFER(&(params->field), sizeof(params->field))

#define READ_TLV(field, type_len) \
	do {if (read_tlv(&(params->field), type_len, &pos, &len)) \
		return -1; } while (0)

int read_tlv(struct wapi_tlv *tlv, const size_t type_len,
				const u8 **pos, size_t *len)
{
	/* type */
	if (*len < type_len) {
		wpa_printf(MSG_ERROR, "WAPI: error reading field (type)");
		return -1;
	}
	if (type_len == 1)
		tlv->type = **pos;
	else if (type_len == 2)
		tlv->type = WPA_GET_BE16(*pos);

	*pos += type_len;
	*len -= type_len;


	/* length */
	if (*len < 2) {
		wpa_printf(MSG_ERROR, "WAPI: error reading field (length)");
		return -1;
	}

	tlv->len = WPA_GET_BE16(*pos);
	*pos += 2;
	*len -= 2;

	/* value */
	if (*len < tlv->len) {
		wpa_printf(MSG_ERROR, "WAPI: error reading field (value)");
		return -1;
	}
	tlv->value = (u8 *)*pos;
	*pos += tlv->len;
	*len -= tlv->len;
	return 0;
}


/* id_len  -  length in octets of id field
 * len_len -  length in octets of length field
 */
#define WRITE_TLV(field, type_len, len_len) \
	do {if (write_tlv(&(params->field), type_len, len_len, &pos, &len)) \
		return -1; } while (0)

int write_tlv(struct wapi_tlv *tlv, size_t type_len,
				size_t len_len, u8 **pos, size_t *len)
{
	/* type */
	if (type_len > *len) {
		wpa_printf(MSG_ERROR, "WAPI: error writing field (type)");
		return -1;
	}
	if (type_len == 1)
		**pos = tlv->type;
	else if (type_len == 2)
		WPA_PUT_BE16(*pos, tlv->type);
	*pos += type_len;
	*len -= type_len;

	/* length */
	if (len_len > *len) {
		wpa_printf(MSG_ERROR, "WAPI: error writing field (length)");
		return -1;
	}
	if (len_len == 1)
		**pos = tlv->len;
	else if (len_len == 2)
		WPA_PUT_BE16(*pos, tlv->len);

	*pos += len_len;
	*len -= len_len;
	if (tlv->len > *len) {
		wpa_printf(MSG_ERROR, "WAPI: error writing field (value)");
		return -1;
	}
	os_memcpy(*pos, tlv->value, tlv->len);
	*pos += tlv->len;
	*len -= tlv->len;
	return 0;
}



/**
 * wapi_decode_msg - decode a wapi message from a byte array
 * @params: struct to hold decoded params
 *          (valid only while input_buffer is in scope)
 * @input_buffer: byte stream to decode
 *                (must be in scope while params is being accessed)
 * @input_buffer_length: length of input buffer
 *
 * params->msg_auth_code is the MAC that appears in the message. not calculated
 * by the supplicant!
 */
int wapi_decode_msg(struct wapi_msg *params,
				const u8 *input_buffer,
				size_t input_buffer_length)
{
	const u8 *pos = input_buffer;
	size_t len = input_buffer_length;
	size_t ie_len;

	READ_SINGLE_FIELD(header);

	switch (params->header.subtype) {
	case WAPI_SUBTYPE_PRE_AUTH_START:
	case WAPI_SUBTYPE_ST_STAKEY_REQ:
		wpa_printf(MSG_ERROR, "WAPI: unsupported message subtype=%d",
				params->header.subtype);
		return -1;

	case WAPI_SUBTYPE_ACCESS_AUT_RESP:
		READ_SINGLE_FIELD(flag);
		READ_SINGLE_FIELD(asue_challenge);
		READ_SINGLE_FIELD(ae_challenge);
		READ_SINGLE_FIELD(access_result);
		READ_SINGLE_FIELD(asue_key_data.len);
		READ_BUFFER(&(params->asue_key_data.content),
				params->asue_key_data.len);
		READ_SINGLE_FIELD(ae_key_data.len);
		READ_BUFFER(&(params->ae_key_data.content),
				params->ae_key_data.len);

		READ_TLV(id_ae, 2);
		READ_TLV(id_asue, 2);

		if (params->flag & WAPI_FLAG_OPTIONAL) {
			wpa_printf(MSG_DEBUG,
					"WAPI: reading optional parameter");

			/* reading auth response */
			params->auth_result.raw_mcvr = pos;

			READ_SINGLE_FIELD(auth_result.type);
			params->auth_result.len = WPA_GET_BE16(pos);
			pos += 2;
			len -= 2;
			READ_SINGLE_FIELD(auth_result.nonce1);
			READ_SINGLE_FIELD(auth_result.nonce2);
			READ_SINGLE_FIELD(auth_result.ver_result1);
			READ_TLV(auth_result.cert1, 2);
			READ_SINGLE_FIELD(auth_result.ver_result2);
			READ_TLV(auth_result.cert2, 2);

			params->auth_result.raw_mcvr_len =
					(size_t)(pos -
					params->auth_result.raw_mcvr);

			/* first signature */
			READ_SINGLE_FIELD(server_signature_asue.id);
			READ_SINGLE_FIELD(server_signature_asue.len);
			READ_TLV(server_signature_asue.identity, 2);
			READ_SINGLE_FIELD(server_signature_asue.signature_alg);
			pos += 2; /* skipping 48 */
			len -= 2;
			READ_BUFFER(&(params->server_signature_asue.ecdsa),
					WAPI_EC_ECDSA_LEN);

			params->signed_content_length =
					(size_t)(pos-input_buffer -
					sizeof(struct wapi_wai_hdr));
			/* second signature */
			READ_SINGLE_FIELD(server_signature_ae.id);
			READ_SINGLE_FIELD(server_signature_ae.len);
			READ_TLV(server_signature_ae.identity, 2);
			READ_SINGLE_FIELD(server_signature_ae.signature_alg);
			pos += 2; /* skipping 48 */
			len -= 2;
			READ_BUFFER(&(params->server_signature_ae.ecdsa),
					WAPI_EC_ECDSA_LEN);
		}
		if (len > 0) {
			params->signed_content_length =
					(size_t)(pos-input_buffer -
					sizeof(struct wapi_wai_hdr));
			READ_SINGLE_FIELD(signature.id);
			READ_SINGLE_FIELD(signature.len);
			READ_TLV(signature.identity, 2);
			READ_SINGLE_FIELD(signature.signature_alg);
			pos += 2; /* skipping 48 */
			len -= 2;
			READ_BUFFER(&(params->signature.ecdsa),
					WAPI_EC_ECDSA_LEN);
		} else {
			os_memcpy(&(params->signature),
					&(params->server_signature_ae),
					sizeof(params->signature));
			os_memset(&(params->server_signature_ae), 0,
					sizeof(params->server_signature_ae));
		}

		wpa_printf(MSG_DEBUG, "WAPI: signed content length: %d",
				params->signed_content_length);

		break;

	case WAPI_SUBTYPE_AUTH_ACTIVACTION:
		READ_SINGLE_FIELD(flag);
		READ_SINGLE_FIELD(auth_id);
		READ_TLV(id_asu, 2);
		READ_TLV(ae_cert, 2);
		READ_TLV(ecdh_params, 1);
		break;

	case WAPI_SUBTYPE_UKEY_NEGO_REQ:
		READ_SINGLE_FIELD(flag);
		READ_SINGLE_FIELD(bkid);
		READ_SINGLE_FIELD(uskid);
		READ_SINGLE_FIELD(addid);
		READ_SINGLE_FIELD(ae_challenge);
		break;

	case WAPI_SUBTYPE_UKEY_NEGO_RES:
		READ_SINGLE_FIELD(flag);
		READ_SINGLE_FIELD(bkid);
		READ_SINGLE_FIELD(uskid);
		READ_SINGLE_FIELD(addid);
		READ_SINGLE_FIELD(asue_challenge);
		READ_SINGLE_FIELD(ae_challenge);
		ie_len = *(pos+1)+2;
		if (ie_len > len) {
			wpa_printf(MSG_ERROR, "WAPI: IE len mismatch");
			return -1;
		}

		if (wapi_parse_ie(pos, ie_len, &(params->wapi_ie)))
			return -1;

		pos += ie_len;
		len -= ie_len;

		READ_SINGLE_FIELD(msg_auth_code);
		break;

	case WAPI_SUBTYPE_UKEY_NEGO_CONFIRM:
		READ_SINGLE_FIELD(flag);
		READ_SINGLE_FIELD(bkid);
		READ_SINGLE_FIELD(uskid);
		READ_SINGLE_FIELD(addid);
		READ_SINGLE_FIELD(asue_challenge);

		ie_len = *(pos+1)+2;
		if (ie_len > len) {
			wpa_printf(MSG_ERROR, "WAPI: IE len mismatch");
			return -1;
		}

		if (wapi_parse_ie(pos, ie_len, &(params->wapi_ie)))
			return -1;

		pos += ie_len;
		len -= ie_len;
		READ_SINGLE_FIELD(msg_auth_code);
		break;

	case WAPI_SUBTYPE_MKEY_STAKEY_ANNOUNCE:
		READ_SINGLE_FIELD(flag);
		READ_SINGLE_FIELD(mskid_stakid);
		READ_SINGLE_FIELD(uskid);
		READ_SINGLE_FIELD(addid);
		READ_SINGLE_FIELD(data_pkt_num);
		READ_SINGLE_FIELD(key_announcment_id);
		READ_SINGLE_FIELD(key_data.len);
		READ_BUFFER(&(params->key_data.content), params->key_data.len);
		READ_SINGLE_FIELD(msg_auth_code);
		break;

	case WAPI_SUBTYPE_MKEY_STAKEY_ANNOUNCE_RES:
		READ_SINGLE_FIELD(flag);
		READ_SINGLE_FIELD(mskid_stakid);
		READ_SINGLE_FIELD(uskid);
		READ_SINGLE_FIELD(addid);
		READ_SINGLE_FIELD(key_announcment_id);
		READ_SINGLE_FIELD(msg_auth_code);
		break;

	default:
		wpa_printf(MSG_ERROR, "WAPI: unknown Message subtype=%d",
				params->header.subtype);
		return -1;
	}

	return 0;
}



/**
 * wapi_encode_msg - encodes a wapi message to byte array
 * @params: WAI message to encode as a byte array
 * @output_buffer: output buffer (acquired by the function)
 * @output_buffer_len: output buffer length
 * @key: key for ecdsa or message auth code (length varies by context)
 * Returns: -1 on error, 0 on success
 *
 * It is the user's responsibility to fill in the correct fields in the struct,
 * according to the message type.
 * The user needs to free the returned buffer by calling os_free()
 * The function constructs WAI message header at the beginning of the buffer &
 * calculates MAC and append to the end.
 */
int wapi_encode_msg(struct wapi_sm *sm, struct wapi_msg *msg,
				u8 **output_buffer,
				size_t *output_buffer_length, u8 *key)
{
	size_t len = WAPI_MAX_MESSAGE_LENGTH;
	u8 *pos = os_malloc(len);
	u8 *mac_start_pos;
	struct wapi_msg *params = msg;
	size_t wapi_ie_len;
	struct wapi_wai_hdr *hdr;
	u8 ecdsa[WAPI_EC_ECDSA_LEN];

	if (pos == NULL)
		wpa_printf(MSG_ERROR, "WAPI: not enough memory "
				"to allocate message buffer");

	(*output_buffer) = pos;

	mac_start_pos = pos;

	hdr = (struct wapi_wai_hdr *) pos;
	WRITE_SINGLE_FIELD(header);

	switch (params->header.subtype) {
	case WAPI_SUBTYPE_PRE_AUTH_START:
	case WAPI_SUBTYPE_ST_STAKEY_REQ:
	case WAPI_SUBTYPE_AUTH_ACTIVACTION:

	case WAPI_SUBTYPE_ACCESS_AUT_RESP:
		wpa_printf(MSG_ERROR, "WAPI: unsupported message subtype=%d",
				params->header.subtype);
		return -1;

	case WAPI_SUBTYPE_ACCESS_AUTH_REQ:
		WRITE_SINGLE_FIELD(flag);
		WRITE_SINGLE_FIELD(auth_id);
		WRITE_SINGLE_FIELD(asue_challenge);
		WRITE_SINGLE_FIELD(asue_key_data.len);
		WRITE_BUFFER(&(params->asue_key_data.content),
				params->asue_key_data.len);
		WRITE_TLV(id_ae, 2, 2);
		WRITE_TLV(asue_cert, 2, 2);
		WRITE_TLV(ecdh_params, 1, 2);

		wapi_ecdsa_sign_frame(key,
				mac_start_pos+sizeof(struct wapi_wai_hdr),
				pos-(mac_start_pos+sizeof(struct wapi_wai_hdr)),
				ecdsa);

		WRITE_SINGLE_FIELD(signature.id);
		WRITE_SINGLE_FIELD(signature.len);
		WRITE_TLV(signature.identity, 2, 2);
		WRITE_SINGLE_FIELD(signature.signature_alg);
		WPA_PUT_BE16(pos, WAPI_EC_ECDSA_LEN);
		pos += 2;
		len -= 2;
		WRITE_BUFFER(ecdsa, WAPI_EC_ECDSA_LEN);

		break;

	case WAPI_SUBTYPE_UKEY_NEGO_REQ:
		WRITE_SINGLE_FIELD(flag);
		WRITE_SINGLE_FIELD(bkid);
		WRITE_SINGLE_FIELD(uskid);
		WRITE_SINGLE_FIELD(addid);
		WRITE_SINGLE_FIELD(ae_challenge);
		break;

	case WAPI_SUBTYPE_UKEY_NEGO_RES:
		WRITE_SINGLE_FIELD(flag);
		WRITE_SINGLE_FIELD(bkid);
		WRITE_SINGLE_FIELD(uskid);
		WRITE_SINGLE_FIELD(addid);
		WRITE_SINGLE_FIELD(asue_challenge);
		WRITE_SINGLE_FIELD(ae_challenge);
		wapi_ie_len = len;
		if (wapi_gen_ie(pos, &wapi_ie_len, params->wapi_ie.akm_suite,
				params->wapi_ie.unicast_suite,
				params->wapi_ie.multicast_suite) != 0) {
			os_free(pos);
			return -1;
		}
		pos += wapi_ie_len;
		len -= wapi_ie_len;
		/* TODO: (wapi) use parameter instead of params->kck */
		wapi_calc_mac(params->kck, mac_start_pos,
				(size_t)(pos - mac_start_pos) + WAPI_MAC_LEN,
				params->msg_auth_code);
		WRITE_SINGLE_FIELD(msg_auth_code);
		break;

	case WAPI_SUBTYPE_UKEY_NEGO_CONFIRM:
		WRITE_SINGLE_FIELD(flag);
		WRITE_SINGLE_FIELD(bkid);
		WRITE_SINGLE_FIELD(uskid);
		WRITE_SINGLE_FIELD(addid);
		WRITE_SINGLE_FIELD(asue_challenge);
		wapi_ie_len = len;
		if (wapi_gen_ie(pos, &wapi_ie_len, params->wapi_ie.akm_suite,
				params->wapi_ie.unicast_suite,
				params->wapi_ie.multicast_suite) != 0) {
			os_free(pos);
			return -1;
		}

		pos += wapi_ie_len;
		len -= wapi_ie_len;
		/* TODO: (wapi) use parameter instead of params->kck */
		wapi_calc_mac(params->kck, mac_start_pos,
				(size_t)(pos - mac_start_pos) + WAPI_MAC_LEN,
				params->msg_auth_code);
		WRITE_SINGLE_FIELD(msg_auth_code);
		break;

	case WAPI_SUBTYPE_MKEY_STAKEY_ANNOUNCE:
		WRITE_SINGLE_FIELD(flag);
		WRITE_SINGLE_FIELD(mskid_stakid);
		WRITE_SINGLE_FIELD(uskid);
		WRITE_SINGLE_FIELD(addid);
		WRITE_SINGLE_FIELD(data_pkt_num);
		WRITE_SINGLE_FIELD(key_announcment_id);
		WRITE_SINGLE_FIELD(key_data.len);
		WRITE_BUFFER(&(params->key_data.content), params->key_data.len);
		wapi_calc_mac(params->kck, mac_start_pos,
				(size_t)(pos - mac_start_pos) + WAPI_MAC_LEN,
				params->msg_auth_code);
		WRITE_SINGLE_FIELD(msg_auth_code);
		break;

	case WAPI_SUBTYPE_MKEY_STAKEY_ANNOUNCE_RES:
		WRITE_SINGLE_FIELD(flag);
		WRITE_SINGLE_FIELD(mskid_stakid);
		WRITE_SINGLE_FIELD(uskid);
		WRITE_SINGLE_FIELD(addid);
		WRITE_SINGLE_FIELD(key_announcment_id);
		wapi_calc_mac(params->kck, mac_start_pos,
				(size_t)(pos - mac_start_pos) + WAPI_MAC_LEN,
				params->msg_auth_code);
		WRITE_SINGLE_FIELD(msg_auth_code);
		break;

	default:
		wpa_printf(MSG_ERROR, "WAPI: unknown message subtype=%d",
				params->header.subtype);
		os_free(pos);
		return -1;
	}

	(*output_buffer_length) = (size_t) (pos - (*output_buffer));

	WPA_PUT_BE16(hdr->version, 1);
	hdr->type = 1;
	WPA_PUT_BE16(hdr->pkt_seq, sm->pkt_seq);
	sm->pkt_seq++;
	WPA_PUT_BE16(hdr->len, pos - (*output_buffer));

	return 0;
}


void wapi_sm_set_config(struct wapi_sm *sm, struct wapi_ssid_config *config)
{
	if (!sm)
		return;

	if (config) {
		if (!sm->cur_ssid)
			sm->cur_ssid = os_malloc(sizeof(*(sm->cur_ssid)));

		sm->cur_ssid->id = config->id;
		sm->cur_ssid->key_mgmt = config->key_mgmt;
		sm->cur_ssid->wapi_psk = config->wapi_psk;
		sm->cur_ssid->wapi_key_type = config->wapi_key_type;
		sm->cur_ssid->as_cert_uri = config->as_cert_uri;
		sm->cur_ssid->user_cert_uri = config->user_cert_uri;
		sm->cur_ssid->user_key_uri = config->user_key_uri;
	} else {
		if (sm->cur_ssid)
			os_free(sm->cur_ssid);
		sm->cur_ssid = NULL;
	}
}

void wapi_sm_deinit(struct wapi_sm *sm)
{
	if (!sm)
		return;
	os_free(sm->ie_ap);
	sm->ie_ap = NULL;
	os_free(sm->ie_assoc);
	sm->ie_assoc = NULL;
	os_free(sm->trunc_msg);
	sm->trunc_msg = NULL;
	if (sm->ecdh_key)
		EC_KEY_free(sm->ecdh_key);
	sm->ecdh_key = NULL;
	if (sm->ae_cert)
		X509_free(sm->ae_cert);
	sm->ae_cert = NULL;

	if (sm->root_cert)
		X509_free(sm->root_cert);
	sm->root_cert = NULL;

	os_free(sm->reentrant_raw);
}


int wapi_tx_wai(struct wapi_sm *sm, const u8 *dest,
			       u8 *msg, size_t msg_len) {
	return sm->ctx->ether_send(sm->ctx->ctx, dest, msg, msg_len);
}


struct wapi_sm *wapi_sm_init(struct wapi_sm *sm_base, struct wapi_sm_ctx *ctx)
{
	struct wapi_sm *sm;

	if (sm_base == NULL) {
		sm = os_malloc(sizeof(*sm));
		if (sm == NULL)
			return NULL;
	} else {
		sm = sm_base;
	}

	os_memset(sm, 0, sizeof(*sm));

	sm->state = WAPI_STATE_INITPSK;
	sm->pkt_seq = 1;
	sm->new_challenge = 1;
	sm->new_ecdh = 1;
	sm->ctx = ctx;
	sm->ecdh_key = EC_KEY_new();
	if (!sm->ecdh_key) {
		wpa_printf(MSG_ERROR, "WAPI: error allocating ecdh key");
		os_free(sm);
		return NULL;
	}
	if (sm->ctx->set_port)
		sm->ctx->set_port(sm->ctx->ctx, 0);

	return sm;
}

void wapi_notify_disassoc(struct wapi_sm *sm)
{
	wapi_sm_deinit(sm);
	wapi_sm_init(sm, sm->ctx);
}
