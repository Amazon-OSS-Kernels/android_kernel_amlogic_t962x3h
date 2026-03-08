/*
 * ramdump.c
 *
 * Copyright 2011-2015 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 */

#include <compiler.h>
#include <ramdump.h>

__WEAK void ramdump_init(void)
{
}

__WEAK int check_ramdump(void)
{
	return 0;
}


__WEAK int ramdump_to_eMMC(void)
{
	return 0;
}

__WEAK int ramdump_in_ram_compress(void)
{
	return 0;
}
