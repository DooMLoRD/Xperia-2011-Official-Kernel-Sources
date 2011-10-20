/* vendor/semc/hardware/felica/felica_cen.h
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

#ifndef _FELICA_CEN_H
#define _FELICA_CEN_H


#define UDELAY_CEN_WRITE 100

struct felica_cen_pfdata;

int felica_cen_probe_func(struct felica_cen_pfdata *);
void felica_cen_remove_func(void);

#endif
