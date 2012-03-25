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
#include "includes.h"
#include "sms4.h"

#ifndef os_memcpy
#define os_memcpy(d, s, n) memcpy((d), (s), (n))
#endif



/* SBOX, FK & CK are constants required for the SMS4
 * algorithm, for keys generation, encryption and decryption */
const u8 SBOX[256] = { 0xd6, 0x90, 0xe9, 0xfe, 0xcc, 0xe1, 0x3d, 0xb7, 0x16, 0xb6, 0x14, 0xc2, 0x28, 0xfb, 0x2c, 0x05,
	0x2b, 0x67, 0x9a, 0x76, 0x2a, 0xbe, 0x04, 0xc3, 0xaa, 0x44, 0x13, 0x26, 0x49, 0x86, 0x06, 0x99,
	0x9c, 0x42, 0x50, 0xf4, 0x91, 0xef, 0x98, 0x7a, 0x33, 0x54, 0x0b, 0x43, 0xed, 0xcf, 0xac, 0x62,
	0xe4, 0xb3, 0x1c, 0xa9, 0xc9, 0x08, 0xe8, 0x95, 0x80, 0xdf, 0x94, 0xfa, 0x75, 0x8f, 0x3f, 0xa6,
	0x47, 0x07, 0xa7, 0xfc, 0xf3, 0x73, 0x17, 0xba, 0x83, 0x59, 0x3c, 0x19, 0xe6, 0x85, 0x4f, 0xa8,
	0x68, 0x6b, 0x81, 0xb2, 0x71, 0x64, 0xda, 0x8b, 0xf8, 0xeb, 0x0f, 0x4b, 0x70, 0x56, 0x9d, 0x35,
	0x1e, 0x24, 0x0e, 0x5e, 0x63, 0x58, 0xd1, 0xa2, 0x25, 0x22, 0x7c, 0x3b, 0x01, 0x21, 0x78, 0x87,
	0xd4, 0x00, 0x46, 0x57, 0x9f, 0xd3, 0x27, 0x52, 0x4c, 0x36, 0x02, 0xe7, 0xa0, 0xc4, 0xc8, 0x9e,
	0xea, 0xbf, 0x8a, 0xd2, 0x40, 0xc7, 0x38, 0xb5, 0xa3, 0xf7, 0xf2, 0xce, 0xf9, 0x61, 0x15, 0xa1,
	0xe0, 0xae, 0x5d, 0xa4, 0x9b, 0x34, 0x1a, 0x55, 0xad, 0x93, 0x32, 0x30, 0xf5, 0x8c, 0xb1, 0xe3,
	0x1d, 0xf6, 0xe2, 0x2e, 0x82, 0x66, 0xca, 0x60, 0xc0, 0x29, 0x23, 0xab, 0x0d, 0x53, 0x4e, 0x6f,
	0xd5, 0xdb, 0x37, 0x45, 0xde, 0xfd, 0x8e, 0x2f, 0x03, 0xff, 0x6a, 0x72, 0x6d, 0x6c, 0x5b, 0x51,
	0x8d, 0x1b, 0xaf, 0x92, 0xbb, 0xdd, 0xbc, 0x7f, 0x11, 0xd9, 0x5c, 0x41, 0x1f, 0x10, 0x5a, 0xd8,
	0x0a, 0xc1, 0x31, 0x88, 0xa5, 0xcd, 0x7b, 0xbd, 0x2d, 0x74, 0xd0, 0x12, 0xb8, 0xe5, 0xb4, 0xb0,
	0x89, 0x69, 0x97, 0x4a, 0x0c, 0x96, 0x77, 0x7e, 0x65, 0xb9, 0xf1, 0x09, 0xc5, 0x6e, 0xc6, 0x84,
	0x18, 0xf0, 0x7d, 0xec, 0x3a, 0xdc, 0x4d, 0x20, 0x79, 0xee, 0x5f, 0x3e, 0xd7, 0xcb, 0x39, 0x48};


const u32 FK[4] = {0xa3b1bac6, 0x56aa3350, 0x677d9197, 0xb27022dc};


const u32 CK[32] = {0x00070e15, 0x1c232a31, 0x383f464d, 0x545b6269,
			0x70777e85, 0x8c939aa1, 0xa8afb6bd, 0xc4cbd2d9,
			0xe0e7eef5, 0xfc030a11, 0x181f262d, 0x343b4249,
			0x50575e65, 0x6c737a81, 0x888f969d, 0xa4abb2b9,
			0xc0c7ced5, 0xdce3eaf1, 0xf8ff060d, 0x141b2229,
			0x30373e45, 0x4c535a61, 0x686f767d, 0x848b9299,
			0xa0a7aeb5, 0xbcc3cad1, 0xd8dfe6ed, 0xf4fb0209,
			0x10171e25, 0x2c333a41, 0x484f565d, 0x646b7279};

#define ROR_SHIFT_LEFT(x, y) ((x << y) | (x >> (32 - y)))
#define SWAP_BYTES_MACRO(input) \
	(((input & 0xff) << 24) | ((input & 0xff00) << 8) | \
	((input & 0xff0000) >> 8) | ((input & 0xff000000) >> 24))

u32 Ttag_Transform(u32 res);


void sms4_BA(const u32 *a, const u32 *rk, u32* out)
{
	u32 i;
	u32 a1, b1, c1, d1, x, z;
	u32 res;
	u8 resA, resB, resC, resD;

	a1 = a[0];
	b1 = a[1];
	c1 = a[2];
	d1 = a[3];

	for (i = 0; i < 32; i++) {
		/* T_Transform */
		/* XOR D[0], D[1], D[2] and rk[i] */
		res = b1 ^ c1 ^ d1 ^ rk[i];
		resA = *((u8 *)(&res));
		resB = *((u8 *)(&res) + 1);
		resC = *((u8 *)(&res) + 2);
		resD = *((u8 *)(&res) + 3);

		/* Transform result by using SBOX */
		z = (SBOX[resA]) | (SBOX[resB] << 8) |
				(SBOX[resC] << 16) | (SBOX[resD] << 24);
		/* And perform T transform by XOR with different rotations */
		z = z ^ ROR_SHIFT_LEFT(z, 2) ^ ROR_SHIFT_LEFT(z, 10) ^
				ROR_SHIFT_LEFT(z, 18) ^ ROR_SHIFT_LEFT(z, 24);

		/* XOR the T transform result with D[0] */
		x = a1 ^ z;

		/* Perform rotation - D[i]=D[i+1], D[i+1]=D[i+2],
		 * D[i+2]=D[i+3], D[i+3]= last XOR result */
		a1 = b1;
		b1 = c1;
		c1 = d1;
		d1 = x;
	}

	/* Output the result of the encryption to D[0-3] */
	out[0] = d1;
	out[1] = c1;
	out[2] = b1;
	out[3] = a1;
}



void sms4_KeyCalc(const u32 *keyIn, u32 *keyOut)
{
	u32 mk0, mk1, mk2, mk3;
	u32 K[36];
	u32 i;

	/* generate encryption key */
	mk0 = *(keyIn+0);
	mk1 = *(keyIn+1);
	mk2 = *(keyIn+2);
	mk3 = *(keyIn+3);

	K[0] = mk0 ^ FK[0];
	K[1] = mk1 ^ FK[1];
	K[2] = mk2 ^ FK[2];
	K[3] = mk3 ^ FK[3];

	for (i = 0; i < 32; i++) {
		K[i+4] = keyOut[i] = (Ttag_Transform(K[i+1] ^ K[i+2] ^
				K[i+3] ^ CK[i]) ^ K[i]);
	}
}


void sms4_decrypt_inner(const u32 *ctext, const u32 *key,
				const u32 *iv, u32 *text)
{
	u32 rk[32];
	u32 eiv[4];
	int i;
	/* creating rk; */
	sms4_KeyCalc(key, rk);

	/* decrypting using iv */
	sms4_BA(iv, rk, eiv);

	for (i = 0; i < 4; i++)
		text[i] = eiv[i] ^ ctext[i];
}


/**
 * decrypts 1 sms4 block (16 octets)
 */
void sms4_decrypt(u8 *key, u8 *iv, u8 *cipher, u8 *clear)
{
	u32 algkey[4];
	u32 algiv[4];
	u32 algcipher[4];
	u32 algclear[4];

	u32 key32[4];
	u32 iv32[4];
	u32 cipher32[4];
	u32 *clear32;
	int i;

	os_memcpy(algkey, key, 4*4);
	os_memcpy(algiv,  iv,  4*4);
	os_memcpy(algcipher, cipher, 4*4);

	/* converting input */
	for (i = 0; i < 4; i++) {
		key32[i] = SWAP_BYTES_MACRO(algkey[i]);
		iv32[i] = SWAP_BYTES_MACRO(algiv[i]);
		cipher32[i] = SWAP_BYTES_MACRO(algcipher[i]);
	}

	sms4_decrypt_inner(cipher32, key32, iv32, (u32 *)algclear);

	/* copying result back */
	clear32 = (u32 *)algclear;
	for (i = 0; i < 4; i++)
		*(clear32+i) = SWAP_BYTES_MACRO(*(clear32+i));

	os_memcpy(clear, algclear, 4*4);
}


/* transformation for the sms4 algo */
u32 Ttag_Transform(u32 res)
{
	u32 x;
	u8 resA, resB, resC, resD;
	resA = *((u8 *)(&res));
	resB = *((u8 *)(&res) + 1);
	resC = *((u8 *)(&res) + 2);
	resD = *((u8 *)(&res) + 3);
	x = (SBOX[resA]) | (SBOX[resB] << 8) |
			(SBOX[resC] << 16) | (SBOX[resD] << 24);
	x = x ^ ROR_SHIFT_LEFT(x, 13) ^ ROR_SHIFT_LEFT(x, 23);
	return x;
}
