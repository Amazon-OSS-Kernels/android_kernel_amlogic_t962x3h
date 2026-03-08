/* SPDX-License-Identifier: GPL-2.0 */

#ifndef _NTP8918_H
#define _NTP8918_H

#define NTP8918_REGISTER_COUNT 1024

#define MCLK                             0x02
#define MVOL                             0x0C
#define C1VOL                            0x17
#define C2VOL                            0x18

struct ntp8918_platform_data {
	int reset_pin;
};

#endif
