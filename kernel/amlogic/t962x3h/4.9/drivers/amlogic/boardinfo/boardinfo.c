/*
 * drivers/amlogic/boardinfo/boardinfo.c
 *
 * Copyright (C) 2017 Amlogic, Inc. All rights reserved.
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

#include <linux/device.h>
#include <linux/amlogic/boardinfo.h>
#define OWNER_NAME "boardinfo"


/*
 * the boardinfo map to AMAZON board id.
 * the map be defined in
 * bl33\board\amlogic\wonka\boardinfo.c
 * boardinfo value:
 * REF_BOARD_ID_TYPE 0
 * HVT_BOARD_ID_TYPE 1
 * EVT_BOARD_ID_TYPE 2
 * DVT_BOARD_ID_TYPE 3
 * PVT_BOARD_ID_TYPE 4
 * AMA_REF_BOARD_ID_TYPE    100
 * AMA_L4_BOARD_ID_TYPE     101
 * AMA_L2_BOARD_ID_TYPE     102
 * PROTO_BOARD_ID_TYPE      103
 */


char boardinfo[10] = "";

static int __init boardinfo_para_setup(char *str)
{
	if (str != NULL)
		sprintf(boardinfo, "%s", str);

	pr_info("boardinfo: %s\n", boardinfo);
	return 0;
}

/*
 * HWID:
 * Meridian: 1110/0111
 * HAZEL: 0011
 * Meridianc: 1001
 * HAZELQ:1111
 */

char PRODUCT_HARDWARE_ID[16];

static int hwboardid = HW_MAX;
static int __init hwid_param(char *line)
{
	strlcpy(PRODUCT_HARDWARE_ID, line, sizeof(PRODUCT_HARDWARE_ID));
	if (strcmp(MERIDIANC_HARDWARE_ID_HVT, PRODUCT_HARDWARE_ID) == 0) {
		hwboardid = HW_MERIDIAN_C;
	} else if ((strcmp(MERIDIAN_HARDWARE_ID_HVT, PRODUCT_HARDWARE_ID) == 0) ||
			(strcmp(MERIDIAN_HARDWARE_ID_DVT, PRODUCT_HARDWARE_ID) == 0)) {
		hwboardid = HW_MERIDIAN;
	} else if (strcmp(HAZELQ_HARDWARE_ID_HVT, PRODUCT_HARDWARE_ID) == 0) {
		hwboardid = HW_HAZELQ;
	} else {
		hwboardid = HW_HAZEL;
	}
	return 1;
}


int  isMeridianc(void)
{
    return hwboardid == HW_MERIDIAN_C;
}

int  isMeridian(void)
{
    return hwboardid == HW_MERIDIAN;
}

int  isHazel(void)
{
    return hwboardid == HW_HAZEL;
}

int isHazelQ(void)
{
	return hwboardid == HW_HAZELQ;
}

__setup("androidboot.hwid=", hwid_param);

__setup("androidboot.boardinfo=", boardinfo_para_setup);
