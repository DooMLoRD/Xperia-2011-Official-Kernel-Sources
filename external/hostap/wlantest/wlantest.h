/*
 * wlantest - IEEE 802.11 protocol monitoring and testing tool
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

#ifndef WLANTEST_H
#define WLANTEST_H

#include "utils/list.h"
#include "common/wpa_common.h"
#include "wlantest_ctrl.h"

struct ieee802_11_elems;
struct radius_msg;
struct ieee80211_hdr;
struct wlantest_bss;

#define MAX_RADIUS_SECRET_LEN 128

struct wlantest_radius_secret {
	struct dl_list list;
	char secret[MAX_RADIUS_SECRET_LEN];
};

struct wlantest_passphrase {
	struct dl_list list;
	char passphrase[64];
	u8 ssid[32];
	size_t ssid_len;
	u8 bssid[ETH_ALEN];
};

struct wlantest_pmk {
	struct dl_list list;
	u8 pmk[32];
};

struct wlantest_wep {
	struct dl_list list;
	size_t key_len;
	u8 key[13];
};

struct wlantest_sta {
	struct dl_list list;
	struct wlantest_bss *bss;
	u8 addr[ETH_ALEN];
	enum {
		STATE1 /* not authenticated */,
		STATE2 /* authenticated */,
		STATE3 /* associated */
	} state;
	u16 aid;
	u8 rsnie[257]; /* WPA/RSN IE */
	int proto;
	int pairwise_cipher;
	int group_cipher;
	int key_mgmt;
	int rsn_capab;
	u8 anonce[32]; /* ANonce from the previous EAPOL-Key msg 1/4 or 3/4 */
	u8 snonce[32]; /* SNonce from the previous EAPOL-Key msg 2/4 */
	struct wpa_ptk ptk; /* Derived PTK */
	int ptk_set;
	u8 rsc_tods[16 + 1][6];
	u8 rsc_fromds[16 + 1][6];
	u8 ap_sa_query_tr[2];
	u8 sta_sa_query_tr[2];
	u32 counters[NUM_WLANTEST_STA_COUNTER];
	u16 assocreq_capab_info;
	u16 assocreq_listen_int;
	u8 *assocreq_ies;
	size_t assocreq_ies_len;

	/* Last ICMP Echo request information */
	u32 icmp_echo_req_src;
	u32 icmp_echo_req_dst;
	u16 icmp_echo_req_id;
	u16 icmp_echo_req_seq;

	le16 seq_ctrl_to_sta[17];
	le16 seq_ctrl_to_ap[17];

	int pwrmgt;
	int pspoll;
};

struct wlantest_tdls {
	struct dl_list list;
	struct wlantest_sta *init;
	struct wlantest_sta *resp;
	struct tpk {
		u8 kck[16];
		u8 tk[16];
	} tpk;
	int link_up;
	u8 dialog_token;
	u8 rsc_init[16 + 1][6];
	u8 rsc_resp[16 + 1][6];
	u32 counters[NUM_WLANTEST_TDLS_COUNTER];
};

struct wlantest_bss {
	struct dl_list list;
	u8 bssid[ETH_ALEN];
	u16 capab_info;
	u16 prev_capab_info;
	u8 ssid[32];
	size_t ssid_len;
	int proberesp_seen;
	int parse_error_reported;
	u8 wpaie[257];
	u8 rsnie[257];
	int proto;
	int pairwise_cipher;
	int group_cipher;
	int mgmt_group_cipher;
	int key_mgmt;
	int rsn_capab;
	struct dl_list sta; /* struct wlantest_sta */
	struct dl_list pmk; /* struct wlantest_pmk */
	u8 gtk[4][32];
	size_t gtk_len[4];
	int gtk_idx;
	u8 rsc[4][6];
	u8 igtk[6][16];
	int igtk_set[6];
	int igtk_idx;
	u8 ipn[6][6];
	u32 counters[NUM_WLANTEST_BSS_COUNTER];
	struct dl_list tdls; /* struct wlantest_tdls */
};

struct wlantest_radius {
	struct dl_list list;
	u32 srv;
	u32 cli;
	struct radius_msg *last_req;
};


#define MAX_CTRL_CONNECTIONS 10

struct wlantest {
	int monitor_sock;
	int monitor_wired;

	int ctrl_sock;
	int ctrl_socks[MAX_CTRL_CONNECTIONS];

	struct dl_list passphrase; /* struct wlantest_passphrase */
	struct dl_list bss; /* struct wlantest_bss */
	struct dl_list secret; /* struct wlantest_radius_secret */
	struct dl_list radius; /* struct wlantest_radius */
	struct dl_list pmk; /* struct wlantest_pmk */
	struct dl_list wep; /* struct wlantest_wep */

	unsigned int rx_mgmt;
	unsigned int rx_ctrl;
	unsigned int rx_data;
	unsigned int fcs_error;

	void *write_pcap; /* pcap_t* */
	void *write_pcap_dumper; /* pcpa_dumper_t */
	struct timeval write_pcap_time;

	u8 last_hdr[30];
	size_t last_len;
	int last_mgmt_valid;
};

int add_wep(struct wlantest *wt, const char *key);
int read_cap_file(struct wlantest *wt, const char *fname);
int read_wired_cap_file(struct wlantest *wt, const char *fname);
int write_pcap_init(struct wlantest *wt, const char *fname);
void write_pcap_deinit(struct wlantest *wt);
void write_pcap_captured(struct wlantest *wt, const u8 *buf, size_t len);
void write_pcap_decrypted(struct wlantest *wt, const u8 *buf1, size_t len1,
			  const u8 *buf2, size_t len2);
void wlantest_process(struct wlantest *wt, const u8 *data, size_t len);
void wlantest_process_prism(struct wlantest *wt, const u8 *data, size_t len);
void wlantest_process_80211(struct wlantest *wt, const u8 *data, size_t len);
void wlantest_process_wired(struct wlantest *wt, const u8 *data, size_t len);
u32 crc32(const u8 *frame, size_t frame_len);
int monitor_init(struct wlantest *wt, const char *ifname);
int monitor_init_wired(struct wlantest *wt, const char *ifname);
void monitor_deinit(struct wlantest *wt);
void rx_mgmt(struct wlantest *wt, const u8 *data, size_t len);
void rx_mgmt_ack(struct wlantest *wt, const struct ieee80211_hdr *hdr);
void rx_data(struct wlantest *wt, const u8 *data, size_t len);
void rx_data_eapol(struct wlantest *wt, const u8 *dst, const u8 *src,
		   const u8 *data, size_t len, int prot);
void rx_data_ip(struct wlantest *wt, const u8 *bssid, const u8 *sta_addr,
		const u8 *dst, const u8 *src, const u8 *data, size_t len,
		const u8 *peer_addr);
void rx_data_80211_encap(struct wlantest *wt, const u8 *bssid,
			 const u8 *sta_addr, const u8 *dst, const u8 *src,
			 const u8 *data, size_t len);

struct wlantest_bss * bss_find(struct wlantest *wt, const u8 *bssid);
struct wlantest_bss * bss_get(struct wlantest *wt, const u8 *bssid);
void bss_deinit(struct wlantest_bss *bss);
void bss_update(struct wlantest *wt, struct wlantest_bss *bss,
		struct ieee802_11_elems *elems);
void bss_flush(struct wlantest *wt);
int bss_add_pmk_from_passphrase(struct wlantest_bss *bss,
				const char *passphrase);
void pmk_deinit(struct wlantest_pmk *pmk);
void tdls_deinit(struct wlantest_tdls *tdls);

struct wlantest_sta * sta_find(struct wlantest_bss *bss, const u8 *addr);
struct wlantest_sta * sta_get(struct wlantest_bss *bss, const u8 *addr);
void sta_deinit(struct wlantest_sta *sta);
void sta_update_assoc(struct wlantest_sta *sta,
		      struct ieee802_11_elems *elems);

u8 * ccmp_decrypt(const u8 *tk, const struct ieee80211_hdr *hdr,
		  const u8 *data, size_t data_len, size_t *decrypted_len);
u8 * ccmp_encrypt(const u8 *tk, u8 *frame, size_t len, size_t hdrlen, u8 *qos,
		  u8 *pn, int keyid, size_t *encrypted_len);
void ccmp_get_pn(u8 *pn, const u8 *data);

u8 * tkip_decrypt(const u8 *tk, const struct ieee80211_hdr *hdr,
		  const u8 *data, size_t data_len, size_t *decrypted_len);
u8 * tkip_encrypt(const u8 *tk, u8 *frame, size_t len, size_t hdrlen, u8 *qos,
		  u8 *pn, int keyid, size_t *encrypted_len);
void tkip_get_pn(u8 *pn, const u8 *data);

u8 * wep_decrypt(struct wlantest *wt, const struct ieee80211_hdr *hdr,
		 const u8 *data, size_t data_len, size_t *decrypted_len);

int ctrl_init(struct wlantest *wt);
void ctrl_deinit(struct wlantest *wt);

int wlantest_inject(struct wlantest *wt, struct wlantest_bss *bss,
		    struct wlantest_sta *sta, u8 *frame, size_t len,
		    enum wlantest_inject_protection prot);

#endif /* WLANTEST_H */
