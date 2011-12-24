#ifndef SMS4_H_
#define SMS4_H_

#include "common.h"

void sms4_decrypt(u8 *key, u8 *iv, u8 *cipher, u8 *clear);

#endif /* SMS4_H_ */
