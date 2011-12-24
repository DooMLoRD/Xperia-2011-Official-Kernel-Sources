/*
 * wlantest frame injection
 * Copyright (c) 2010, Jouni Malinen <j@w1.fi>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Alternatively, this software may be distributed under the terms of BSD
 * license.
 *
 * See README and COPYING for more details.
 */

#include "utils/includes.h"

#include "utils/common.h"
#include "common/defs.h"
#include "common/ieee802_11_defs.h"
#include "crypto/aes_wrap.h"
#include "wlantest.h"


static int inject_frame(int s, const void *data, size_t len)
{
#define	IEEE80211_RADIOTAP_F_FRAG	0x08
	unsigned char rtap_hdr[] = {
		0x00, 0x00, /* radiotap version */
		0x0e, 0x00, /* radiotap length */
		0x02, 0xc0, 0x00, 0x00, /* bmap: flags, tx and rx flags */
		IEEE80211_RADIOTAP_F_FRAG, /* F_FRAG (fragment if required) */
		0x00,       /* padding */
		0x00, 0x00, /* RX and TX flags to indicate that */
		0x00, 0x00, /* this is the injected frame directly */
	};
	struct iovec iov[2] = {
		{
			.iov_base = &rtap_hdr,
			.iov_len = sizeof(rtap_hdr),
		},
		{
			.iov_base = (void *) data,
			.iov_len = len,
		}
	};
	struct msghdr msg = {
		.msg_name = NULL,
		.msg_namelen = 0,
		.msg_iov = iov,
		.msg_iovlen = 2,
		.msg_control = NULL,
		.msg_controllen = 0,
		.msg_flags = 0,
	};
	int ret;

	ret = sendmsg(s, &msg, 0);
	if (ret < 0)
		perror("sendmsg");
	return ret;
}


static int is_robust_mgmt(u8 *frame, size_t len)
{
	struct ieee80211_mgmt *mgmt;
	u16 fc, stype;
	if (len < 24)
		return 0;
	mgmt = (struct ieee80211_mgmt *) frame;
	fc = le_to_host16(mgmt->frame_control);
	if (WLAN_FC_GET_TYPE(fc) != WLAN_FC_TYPE_MGMT)
		return 0;
	stype = WLAN_FC_GET_STYPE(fc);
	if (stype == WLAN_FC_STYPE_DEAUTH || stype == WLAN_FC_STYPE_DISASSOC)
		return 1;
	if (stype == WLAN_FC_STYPE_ACTION) {
		if (len < 25)
			return 0;
		if (mgmt->u.action.category != WLAN_ACTION_PUBLIC)
			return 1;
	}
	return 0;
}


static int wlantest_inject_bip(struct wlantest *wt, struct wlantest_bss *bss,
			       u8 *frame, size_t len, int incorrect_key)
{
	u8 *prot, *pos, *buf;
	u8 mic[16];
	u8 dummy[16];
	int ret;
	u16 fc;
	struct ieee80211_hdr *hdr;
	size_t plen;

	if (!bss->igtk_set[bss->igtk_idx])
		return -1;

	plen = len + 18;
	prot = os_malloc(plen);
	if (prot == NULL)
		return -1;
	os_memcpy(prot, frame, len);
	pos = prot + len;
	*pos++ = WLAN_EID_MMIE;
	*pos++ = 16;
	WPA_PUT_LE16(pos, bss->igtk_idx);
	pos += 2;
	inc_byte_array(bss->ipn[bss->igtk_idx], 6);
	os_memcpy(pos, bss->ipn[bss->igtk_idx], 6);
	pos += 6;
	os_memset(pos, 0, 8); /* MIC */

	buf = os_malloc(plen + 20 - 24);
	if (buf == NULL) {
		os_free(prot);
		return -1;
	}

	/* BIP AAD: FC(masked) A1 A2 A3 */
	hdr = (struct ieee80211_hdr *) frame;
	fc = le_to_host16(hdr->frame_control);
	fc &= ~(WLAN_FC_RETRY | WLAN_FC_PWRMGT | WLAN_FC_MOREDATA);
	WPA_PUT_LE16(buf, fc);
	os_memcpy(buf + 2, hdr->addr1, 3 * ETH_ALEN);
	os_memcpy(buf + 20, prot + 24, plen - 24);
	wpa_hexdump(MSG_MSGDUMP, "BIP: AAD|Body(masked)", buf, plen + 20 - 24);
	/* MIC = L(AES-128-CMAC(AAD || Frame Body(masked)), 0, 64) */
	os_memset(dummy, 0x11, sizeof(dummy));
	if (omac1_aes_128(incorrect_key ? dummy : bss->igtk[bss->igtk_idx],
			  buf, plen + 20 - 24, mic) < 0) {
		os_free(prot);
		os_free(buf);
		return -1;
	}
	os_free(buf);

	os_memcpy(pos, mic, 8);
	wpa_hexdump(MSG_DEBUG, "BIP MMIE MIC", pos, 8);

	ret = inject_frame(wt->monitor_sock, prot, plen);
	os_free(prot);

	return (ret < 0) ? -1 : 0;
}


static int wlantest_inject_prot_bc(struct wlantest *wt,
				   struct wlantest_bss *bss,
				   u8 *frame, size_t len, int incorrect_key)
{
	u8 *crypt;
	size_t crypt_len;
	int ret;
	u8 dummy[64];
	u8 *pn;
	struct ieee80211_hdr *hdr;
	u16 fc;
	int hdrlen;

	hdr = (struct ieee80211_hdr *) frame;
	hdrlen = 24;
	fc = le_to_host16(hdr->frame_control);

	if (!bss->gtk_len[bss->gtk_idx])
		return -1;

	if ((fc & (WLAN_FC_TODS | WLAN_FC_FROMDS)) ==
	    (WLAN_FC_TODS | WLAN_FC_FROMDS))
		hdrlen += ETH_ALEN;
	pn = bss->rsc[bss->gtk_idx];
	inc_byte_array(pn, 6);

	os_memset(dummy, 0x11, sizeof(dummy));
	if (bss->group_cipher == WPA_CIPHER_TKIP)
		crypt = tkip_encrypt(incorrect_key ? dummy :
				     bss->gtk[bss->gtk_idx],
				     frame, len, hdrlen, NULL, pn,
				     bss->gtk_idx, &crypt_len);
	else
		crypt = ccmp_encrypt(incorrect_key ? dummy :
				     bss->gtk[bss->gtk_idx],
				     frame, len, hdrlen, NULL, pn,
				     bss->gtk_idx, &crypt_len);

	if (crypt == NULL)
		return -1;

	ret = inject_frame(wt->monitor_sock, crypt, crypt_len);
	os_free(crypt);

	return (ret < 0) ? -1 : 0;
}


static int wlantest_inject_prot(struct wlantest *wt, struct wlantest_bss *bss,
				struct wlantest_sta *sta, u8 *frame,
				size_t len, int incorrect_key)
{
	u8 *crypt;
	size_t crypt_len;
	int ret;
	u8 dummy[64];
	u8 *pn;
	struct ieee80211_hdr *hdr;
	u16 fc;
	int tid = 0;
	u8 *qos = NULL;
	int hdrlen;
	struct wlantest_tdls *tdls = NULL;
	const u8 *tk = NULL;

	hdr = (struct ieee80211_hdr *) frame;
	hdrlen = 24;
	fc = le_to_host16(hdr->frame_control);

	if (WLAN_FC_GET_TYPE(fc) == WLAN_FC_TYPE_DATA &&
	    (fc & (WLAN_FC_TODS | WLAN_FC_FROMDS)) == 0) {
		struct wlantest_sta *sta2;
		bss = bss_get(wt, hdr->addr3);
		if (bss == NULL) {
			wpa_printf(MSG_DEBUG, "No BSS found for TDLS "
				   "injection");
			return -1;
		}
		sta = sta_find(bss, hdr->addr2);
		sta2 = sta_find(bss, hdr->addr1);
		if (sta == NULL || sta2 == NULL) {
			wpa_printf(MSG_DEBUG, "No stations found for TDLS "
				   "injection");
			return -1;
		}
		dl_list_for_each(tdls, &bss->tdls, struct wlantest_tdls, list)
		{
			if ((tdls->init == sta && tdls->resp == sta2) ||
			    (tdls->init == sta2 && tdls->resp == sta)) {
				if (!tdls->link_up)
					wpa_printf(MSG_DEBUG, "TDLS: Link not "
						   "up, but injecting Data "
						   "frame on direct link");
				tk = tdls->tpk.tk;
				break;
			}
		}
	}

	if (tk == NULL && sta == NULL) {
		if (WLAN_FC_GET_TYPE(fc) == WLAN_FC_TYPE_MGMT)
			return wlantest_inject_bip(wt, bss, frame, len,
						   incorrect_key);
		return wlantest_inject_prot_bc(wt, bss, frame, len,
					       incorrect_key);
	}

	if (tk == NULL && !sta->ptk_set) {
		wpa_printf(MSG_DEBUG, "No key known for injection");
		return -1;
	}

	if (WLAN_FC_GET_TYPE(fc) == WLAN_FC_TYPE_MGMT)
		tid = 16;
	else if (WLAN_FC_GET_TYPE(fc) == WLAN_FC_TYPE_DATA) {
		if ((fc & (WLAN_FC_TODS | WLAN_FC_FROMDS)) ==
		    (WLAN_FC_TODS | WLAN_FC_FROMDS))
			hdrlen += ETH_ALEN;
		if (WLAN_FC_GET_STYPE(fc) & 0x08) {
			qos = frame + hdrlen;
			hdrlen += 2;
			tid = qos[0] & 0x0f;
		}
	}
	if (tk) {
		if (os_memcmp(hdr->addr2, tdls->init->addr, ETH_ALEN) == 0)
			pn = tdls->rsc_init[tid];
		else
			pn = tdls->rsc_resp[tid];
	} else if (os_memcmp(hdr->addr2, bss->bssid, ETH_ALEN) == 0)
		pn = sta->rsc_fromds[tid];
	else
		pn = sta->rsc_tods[tid];
	inc_byte_array(pn, 6);

	os_memset(dummy, 0x11, sizeof(dummy));
	if (tk) 
		crypt = ccmp_encrypt(incorrect_key ? dummy : tk,
				     frame, len, hdrlen, qos, pn, 0,
				     &crypt_len);
	else if (sta->pairwise_cipher == WPA_CIPHER_TKIP)
		crypt = tkip_encrypt(incorrect_key ? dummy : sta->ptk.tk1,
				     frame, len, hdrlen, qos, pn, 0,
				     &crypt_len);
	else
		crypt = ccmp_encrypt(incorrect_key ? dummy : sta->ptk.tk1,
				     frame, len, hdrlen, qos, pn, 0,
				     &crypt_len);

	if (crypt == NULL) {
		wpa_printf(MSG_DEBUG, "Frame encryption failed");
		return -1;
	}

	wpa_hexdump(MSG_DEBUG, "Inject frame (encrypted)", crypt, crypt_len);
	ret = inject_frame(wt->monitor_sock, crypt, crypt_len);
	os_free(crypt);
	wpa_printf(MSG_DEBUG, "inject_frame for protected frame: %d", ret);

	return (ret < 0) ? -1 : 0;
}


int wlantest_inject(struct wlantest *wt, struct wlantest_bss *bss,
		    struct wlantest_sta *sta, u8 *frame, size_t len,
		    enum wlantest_inject_protection prot)
{
	int ret;
	struct ieee80211_hdr *hdr;
	u16 fc;
	int protectable, protect = 0;

	wpa_hexdump(MSG_DEBUG, "Inject frame", frame, len);
	if (wt->monitor_sock < 0) {
		wpa_printf(MSG_INFO, "Cannot inject frames when monitor "
			   "interface is not in use");
		return -1;
	}

	hdr = (struct ieee80211_hdr *) frame;
	fc = le_to_host16(hdr->frame_control);
	protectable = WLAN_FC_GET_TYPE(fc) == WLAN_FC_TYPE_DATA ||
		is_robust_mgmt(frame, len);

	if (prot == WLANTEST_INJECT_PROTECTED ||
	    prot == WLANTEST_INJECT_INCORRECT_KEY) {
		if (!sta &&
		    ((WLAN_FC_GET_TYPE(fc) == WLAN_FC_TYPE_MGMT &&
		      !bss->igtk_set[bss->igtk_idx]) ||
		     (WLAN_FC_GET_TYPE(fc) == WLAN_FC_TYPE_DATA &&
		      !bss->gtk_len[bss->gtk_idx]))) {
			wpa_printf(MSG_INFO, "No GTK/IGTK known for "
				   MACSTR " to protect the injected "
				   "frame", MAC2STR(bss->bssid));
			return -1;
		}
		if (sta && !sta->ptk_set) {
			wpa_printf(MSG_INFO, "No PTK known for the STA " MACSTR
				   " to encrypt the injected frame",
				   MAC2STR(sta->addr));
			return -1;
		}
		protect = 1;
	} else if (protectable && prot != WLANTEST_INJECT_UNPROTECTED) {
		if (sta && sta->ptk_set)
			protect = 1;
		else if (!sta) {
			if (WLAN_FC_GET_TYPE(fc) == WLAN_FC_TYPE_DATA &&
			    bss->gtk_len[bss->gtk_idx])
				protect = 1;
			if (WLAN_FC_GET_TYPE(fc) == WLAN_FC_TYPE_MGMT &&
			    bss->igtk_set[bss->igtk_idx])
				protect = 1;
		}
	}

	if (protect)
		return wlantest_inject_prot(
			wt, bss, sta, frame, len,
			prot == WLANTEST_INJECT_INCORRECT_KEY);

	ret = inject_frame(wt->monitor_sock, frame, len);
	wpa_printf(MSG_DEBUG, "inject_frame for unprotected frame: %d", ret);
	return (ret < 0) ? -1 : 0;
}
