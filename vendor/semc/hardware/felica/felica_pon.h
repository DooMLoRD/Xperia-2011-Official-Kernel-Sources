/* vendor/semc/hardware/felica/felica_pon.h
 *
 * Copyright (C) 2010 Sony Ericsson Mobile Communications AB.
 *
 * Author: Hiroaki Kuriyama <Hiroaki.Kuriyama@sonyericsson.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2, as
 * published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 */

#ifndef _FELICA_PON_H
#define _FELICA_PON_H

struct felica_pon_pfdata;

int felica_pon_probe_func(struct felica_pon_pfdata *);
void felica_pon_remove_func(void);

#endif
