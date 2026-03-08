/*
 *Copyright (c) 2019 Amazon.com, Inc. or its affiliates.  All rights reserved.
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "debug.h"
#define UFBL_BCB_BOOTCTRL_INTERNAL
#include "bootctrl.h"
#include "bcb.h"


// TODO: actually use gtest
#define ASSERT_EQ(a, b) assert(a == b)
#define ASSERT_STREQ(a, b) assert(a != NULL && b != NULL && strcmp(a, b) == 0)

// VISIBLE_FOR_TESTING functions in bcb.c
extern void create_default_metadata(struct bootloader_control *bc, int default_active_slot);
extern int get_active_slot(const struct bootloader_control *bc);
extern void hex_dump_bootloader_control(int log_level, const char *banner,
                                        const struct bootloader_control *bc);
extern void print_bootloader_control(int log_level, const struct bootloader_control *bc);

static bool mock_read_write_true(void *buf, size_t offset, size_t size)
{
    return true;
}

static bool mock_read_write_false(void *buf, size_t offset, size_t size)
{
    return false;
}

static bool mock_read_test_a(void *buf, size_t offset, size_t size)
{
    create_default_metadata(buf, 0);
    return true;
}

static bool (*s_mock_read) (void *buf, size_t offset, size_t size) = mock_read_write_true;
static bool (*s_mock_write) (void *buf, size_t offset, size_t size) = mock_read_write_true;

static void test_create_default_metadata_with_slot(int slot)
{
    struct bootloader_control bc;
    int active_slot = -1;

    dprintf(INFO, "==== test_create_default_metadata_with_slot %d ====\n", slot);
    create_default_metadata(&bc, slot);
    active_slot = get_active_slot(&bc);
    ASSERT_EQ(slot, active_slot);
    hex_dump_bootloader_control(INFO, "test default metadata", &bc);
    print_bootloader_control(INFO, &bc);
}

static void test_create_default_metadata()
{
    test_create_default_metadata_with_slot(0);
    test_create_default_metadata_with_slot(1);
}

static void test_ufbl_set_active_slot()
{
    ASSERT_EQ(0, ufbl_bcb_set_active_slot(0));
    ASSERT_EQ(0, ufbl_bcb_set_active_slot(1));
    ASSERT_EQ(-1, ufbl_bcb_set_active_slot(2));
}

static void test_ufbl_get_active_slot_suffix()
{
    const char *suffix;

    s_mock_read = mock_read_write_false;
    suffix = ufbl_bcb_get_active_slot_suffix(NULL);
    ASSERT_EQ(NULL, suffix);

    s_mock_read = mock_read_test_a;
    suffix = ufbl_bcb_get_active_slot_suffix(NULL);
    ASSERT_STREQ("_a", suffix);

    s_mock_read = mock_read_write_true;
}

static void test_ufbl_get_info()
{
    struct ufbl_bcb bcb;
    int ret;

    s_mock_read = mock_read_test_a;
    ret = ufbl_bcb_get_info(&bcb);
    ASSERT_EQ(0, ret);
    ASSERT_EQ(2, bcb.slot_count);
    ASSERT_EQ(0, bcb.current_slot);
    ASSERT_EQ('a', bcb.slot_info[0].suffix);
    ASSERT_EQ('b', bcb.slot_info[1].suffix);
    s_mock_read = mock_read_write_true;
}

int bcb_platform_read(void *buf, size_t offset, size_t size)
{
    return s_mock_read(buf, offset, size);
}

int bcb_platform_write(void *buf, size_t offset, size_t size)
{
    return s_mock_write(buf, offset, size);
}

int main(int argc, char *argv[])
{
    dprintf(INFO, "UFBL BCB unit tests begin\n");

    test_create_default_metadata();
    test_ufbl_set_active_slot();
    test_ufbl_get_active_slot_suffix();
    test_ufbl_get_info();

    dprintf(INFO, "UFBL BCB unit tests finish\n");
    return 0;
}
