/*
 * ./include/linux/amlogic/boardinfo.h
 *
 * Copyright (C) 2022 Amlogic, Inc. All rights reserved.
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

#ifndef _BOARDINFO_H
#define _BOARDINFO_H

/*
 * HWID:
 * Meridian: 1110
 * HAZEL: 0011
 *
 */
#define HAZEL_HARDWARE_ID	"0011"
#define MERIDIAN_HARDWARE_ID_HVT	"1110"
#define MERIDIAN_HARDWARE_ID_DVT	"0111"
#define MERIDIANC_HARDWARE_ID_HVT        "1001"
extern char PRODUCT_HARDWARE_ID[16];

typedef enum {
    HW_UNDEF,
    HW_HAZEL = 1,
    HW_MERIDIAN,
    HW_MERIDIAN_C,
    HW_MAX
} HW_BOARD_T;

int isMeridianc(void);
int isMeridian(void);
int  isHazel(void);
#endif /* _BOARDINFO_H */
