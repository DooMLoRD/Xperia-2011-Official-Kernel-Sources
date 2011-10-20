/* vendor/semc/hardware/felica/felica_rws.h
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

#ifndef _FELICA_RWS_H
#define _FELICA_RWS_H

#define PARAM_OFF 0
#define PARAM_ON  1

extern unsigned int st_usbcon;
extern unsigned int st_airplane;
extern unsigned int ta_rwusb;

int felica_rws_probe_func(void);
void felica_rws_remove_func(void);

#endif
