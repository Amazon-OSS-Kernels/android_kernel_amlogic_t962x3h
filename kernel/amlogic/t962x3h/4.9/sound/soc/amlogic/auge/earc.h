/*
 * sound/soc/amlogic/auge/earc.h
 *
 * Copyright (C) 2019 Amlogic, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */

#ifndef __EARC_H__
#define __EARC_H__
#include <linux/amlogic/media/sound/iec_info.h>
#include "earc_hw.h"
/* earc probe is at arch_initcall stage which is earlier to normal driver */
bool is_earc_spdif(void);
void aml_earctx_enable(bool enable);
int sharebuffer_earctx_prepare(struct snd_pcm_substream *substream,
	struct frddr *fr, enum aud_codec_types type, int lane_i2s);
bool aml_get_earctx_enable(void);
bool get_earcrx_chnum_mult_mode(void);
enum attend_type aml_get_earctx_connected_device_type(void);
bool aml_get_earctx_reset_hpd(void);
void aml_earctx_enable_d2a(int enable);
void aml_earctx_dmac_mute(int enable);

#endif

