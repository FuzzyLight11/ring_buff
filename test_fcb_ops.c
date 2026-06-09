/**
 * @file  test_fcb_ops.c
 * @brief Bug-revealing tests targeting config validation, seq_diff contract,
 *        flash error propagation, CRC corruption recovery, trim edge cases,
 *        and append lifecycle gaps.
 */

#include "test_fcb_ops.h"
#include "unity.h"
#include "fcb/fcb.h"
#include "fcb/fcb_append.h"
#include "flash_ops/flash_ops.h"
#include "flash_mem/flash_mem.h"

#include <stdint.h>
#include <string.h>

/* ================================================================== */
/*  Test function declarations                                         */
/* ================================================================== */

/* Config validation */
void test_fcb_init_null_fcb_returns_invalid(void);
void test_fcb_init_null_cfg_returns_invalid(void);
void test_fcb_init_zero_sectors_returns_invalid(void);
void test_fcb_init_above_max_sectors_returns_invalid(void);
void test_fcb_init_sector_size_below_minimum_returns_invalid(void);
void test_fcb_init_null_flash_read_returns_invalid(void);
void test_fcb_init_null_flash_write_returns_invalid(void);
void test_fcb_init_null_flash_erase_returns_invalid(void);

/* fcb_seq_diff contract */
void test_fcb_seq_diff_a_newer_than_b(void);
void test_fcb_seq_diff_a_older_than_b(void);
void test_fcb_seq_diff_equal_returns_zero(void);
void test_fcb_seq_diff_wrap_forward(void);
void test_fcb_seq_diff_wrap_backward(void);

/* State guard checks */
void test_fcb_write_bad_magic_returns_invalid(void);
void test_fcb_delete_null_fcb_returns_invalid(void);
void test_fcb_trim_null_fcb_returns_invalid(void);
void test_fcb_is_full_null_returns_true(void);
void test_fcb_is_empty_null_returns_true(void);

/* Minimum payload boundary */
void test_fcb_write_one_byte_round_trip(void);

/* Flash error injection — BUG: raw -1 propagated instead of FCB_ERR_FLASH */
void test_fcb_write_flash_write_error_returns_fcb_err_flash(void);
void test_fcb_trim_flash_erase_error_returns_fcb_err_flash(void);

/* CRC corruption auto-skip */
void test_fcb_read_crc_corrupted_returns_empty_increments_count(void);
void test_fcb_read_crc_corrupted_skips_to_next_valid(void);

/* Trim edge cases */
void test_fcb_trim_on_empty_fcb_returns_ok(void);
void test_fcb_trim_advances_read_ptr_when_pointing_at_erased_sector(void);

/* Append lifecycle gaps */
void test_fcb_append_begin_null_entry_returns_invalid(void);
void test_fcb_append_write_zero_len_returns_invalid(void);
void test_fcb_append_abort_with_no_writes_subsequent_write_ok(void);
void test_fcb_append_double_abort_second_returns_invalid(void);

/* Remount persistence */
void test_fcb_remount_after_delete_skips_consumed_records(void);

/* ================================================================== */
/*  Section 1: Config validation                                       */
/* ================================================================== */

void test_fcb_init_null_fcb_returns_invalid(void)
{
    FcbConfig cfg;
    setup_config(&cfg);

    TEST_ASSERT_EQUAL_INT(FCB_INVALID_ARG, fcb_init(NULL, &cfg));
}

void test_fcb_init_null_cfg_returns_invalid(void)
{
    Fcb fcb;

    TEST_ASSERT_EQUAL_INT(FCB_INVALID_ARG, fcb_init(&fcb, NULL));
}

void test_fcb_init_zero_sectors_returns_invalid(void)
{
    Fcb       fcb;
    FcbConfig cfg;
    setup_config(&cfg);
    cfg.num_sectors = 0;

    TEST_ASSERT_EQUAL_INT(FCB_INVALID_ARG, fcb_init(&fcb, &cfg));
}

void test_fcb_init_above_max_sectors_returns_invalid(void)
{
    Fcb       fcb;
    FcbConfig cfg;
    setup_config(&cfg);
    cfg.num_sectors = FCB_MAX_SECTORS + 1;

    TEST_ASSERT_EQUAL_INT(FCB_INVALID_ARG, fcb_init(&fcb, &cfg));
}

void test_fcb_init_sector_size_below_minimum_returns_invalid(void)
{
    Fcb       fcb;
    FcbConfig cfg;
    setup_config(&cfg);
    /* Minimum valid = FCB_SECTOR_HDR_SIZE + FCB_RECORD_HDR_SIZE + 1 = 21.
     * Setting to 20 must be rejected. */
    cfg.sector_size = FCB_SECTOR_HDR_SIZE + FCB_RECORD_HDR_SIZE;

    TEST_ASSERT_EQUAL_INT(FCB_INVALID_ARG, fcb_init(&fcb, &cfg));
}

void test_fcb_init_null_flash_read_returns_invalid(void)
{
    Fcb       fcb;
    FcbConfig cfg;
    setup_config(&cfg);
    cfg.flash_read = NULL;

    TEST_ASSERT_EQUAL_INT(FCB_INVALID_ARG, fcb_init(&fcb, &cfg));
}

void test_fcb_init_null_flash_write_returns_invalid(void)
{
    Fcb       fcb;
    FcbConfig cfg;
    setup_config(&cfg);
    cfg.flash_write = NULL;

    TEST_ASSERT_EQUAL_INT(FCB_INVALID_ARG, fcb_init(&fcb, &cfg));
}

void test_fcb_init_null_flash_erase_returns_invalid(void)
{
    Fcb       fcb;
    FcbConfig cfg;
    setup_config(&cfg);
    cfg.flash_erase = NULL;

    TEST_ASSERT_EQUAL_INT(FCB_INVALID_ARG, fcb_init(&fcb, &cfg));
}

/* ================================================================== */
/*  Section 2: fcb_seq_diff contract                                   */
/* ================================================================== */

void test_fcb_seq_diff_a_newer_than_b(void)
{
    TEST_ASSERT_TRUE(fcb_seq_diff(5, 3) > 0);
}

void test_fcb_seq_diff_a_older_than_b(void)
{
    TEST_ASSERT_TRUE(fcb_seq_diff(3, 5) < 0);
}

void test_fcb_seq_diff_equal_returns_zero(void)
{
    TEST_ASSERT_EQUAL_INT32(0, fcb_seq_diff(7, 7));
}

/* seq 0 wraps past UINT32_MAX — 0 should be considered *newer* than UINT32_MAX */
void test_fcb_seq_diff_wrap_forward(void)
{
    TEST_ASSERT_TRUE(fcb_seq_diff(0U, 0xFFFFFFFFU) > 0);
}

/* UINT32_MAX is *older* than 0 after wrap */
void test_fcb_seq_diff_wrap_backward(void)
{
    TEST_ASSERT_TRUE(fcb_seq_diff(0xFFFFFFFFU, 0U) < 0);
}

/* ================================================================== */
/*  Section 3: State guard checks                                      */
/* ================================================================== */

void test_fcb_write_bad_magic_returns_invalid(void)
{
    /* Zero-initialised Fcb has magic = 0 and is_mounted = false. */
    Fcb     fcb;
    uint8_t data[] = "test";
    memset(&fcb, 0, sizeof(fcb));

    TEST_ASSERT_EQUAL_INT(FCB_INVALID_ARG, fcb_write(&fcb, data, sizeof(data)));
}

void test_fcb_delete_null_fcb_returns_invalid(void)
{
    TEST_ASSERT_EQUAL_INT(FCB_INVALID_ARG, fcb_delete(NULL));
}

void test_fcb_trim_null_fcb_returns_invalid(void)
{
    TEST_ASSERT_EQUAL_INT(FCB_INVALID_ARG, fcb_trim(NULL));
}

void test_fcb_is_full_null_returns_true(void)
{
    TEST_ASSERT_TRUE(fcb_is_full(NULL));
}

void test_fcb_is_empty_null_returns_true(void)
{
    TEST_ASSERT_TRUE(fcb_is_empty(NULL));
}

/* ================================================================== */
/*  Section 4: Minimum payload boundary                               */
/* ================================================================== */

void test_fcb_write_one_byte_round_trip(void)
{
    flash_init(NULL);

    Fcb       fcb;
    FcbConfig cfg;
    setup_config(&cfg);
    TEST_ASSERT_EQUAL_INT(FCB_OK, fcb_init(&fcb, &cfg));

    const uint8_t payload[1] = {0xA5U};
    TEST_ASSERT_EQUAL_INT(FCB_OK, fcb_write(&fcb, payload, 1));

    uint8_t read_buf[1];
    size_t  len_out = 0;
    TEST_ASSERT_EQUAL_INT(FCB_OK, fcb_read(&fcb, read_buf, sizeof(read_buf), &len_out));
    TEST_ASSERT_EQUAL_size_t(1, len_out);
    TEST_ASSERT_EQUAL_MEMORY(payload, read_buf, 1);

    flash_deinit();
}

/* ================================================================== */
/*  Section 5: Flash error propagation                                 */
/*                                                                     */
/*  BUG: fcb_flash_write / fcb_flash_erase forward the raw callback   */
/*  return value (-1) instead of mapping it to FCB_ERR_FLASH.         */
/*  These tests assert the documented contract; they FAIL on current   */
/*  code, confirming the error-code mapping defect.                    */
/* ================================================================== */

void test_fcb_write_flash_write_error_returns_fcb_err_flash(void)
{
    flash_init(NULL);

    Fcb       fcb;
    FcbConfig cfg;
    setup_config(&cfg);
    TEST_ASSERT_EQUAL_INT(FCB_OK, fcb_init(&fcb, &cfg));

    /* Swap write callback to the error-injecting variant. */
    fcb.config.flash_write = sim_flash_program_inject_error;
    injectProgramError     = 1;

    uint8_t data[] = "trigger";
    int     rc     = fcb_write(&fcb, data, sizeof(data));

    /* CONTRACT: flash failure must surface as FCB_ERR_FLASH.
     * Current code returns -1 (raw callback result) — this assertion fails. */
    TEST_ASSERT_EQUAL_INT_MESSAGE(
        FCB_ERR_FLASH, rc,
        "BUG: flash write error returned raw -1 instead of FCB_ERR_FLASH");

    flash_deinit();
}

void test_fcb_trim_flash_erase_error_returns_fcb_err_flash(void)
{
    flash_init(NULL);

    Fcb       fcb;
    FcbConfig cfg;
    setup_config(&cfg);
    TEST_ASSERT_EQUAL_INT(FCB_OK, fcb_init(&fcb, &cfg));

    uint8_t data[] = "fill";
    TEST_ASSERT_EQUAL_INT(FCB_OK, fcb_write(&fcb, data, sizeof(data)));

    /* Swap erase callback to the error-injecting variant. */
    fcb.config.flash_erase = sim_flash_erase_inject_error;
    injectEraseError       = 1;

    int rc = fcb_trim(&fcb);

    /* CONTRACT: flash failure must surface as FCB_ERR_FLASH.
     * Current code returns -1 (raw callback result) — this assertion fails. */
    TEST_ASSERT_EQUAL_INT_MESSAGE(
        FCB_ERR_FLASH, rc,
        "BUG: flash erase error returned raw -1 instead of FCB_ERR_FLASH");

    flash_deinit();
}

/* ================================================================== */
/*  Section 6: CRC corruption auto-skip                               */
/* ================================================================== */

void test_fcb_read_crc_corrupted_returns_empty_increments_count(void)
{
    flash_init(NULL);

    Fcb       fcb;
    FcbConfig cfg;
    setup_config(&cfg);
    TEST_ASSERT_EQUAL_INT(FCB_OK, fcb_init(&fcb, &cfg));

    /* Write a single record: CRC byte lands at
     * sector_start(0) + FCB_SECTOR_HDR_SIZE + FCB_RECORD_HDR_SIZE + payload_len */
    const uint8_t payload[] = "corrupt_me";
    const size_t  pay_len   = sizeof(payload);
    TEST_ASSERT_EQUAL_INT(FCB_OK, fcb_write(&fcb, payload, pay_len));

    uint32_t crc_addr = FCB_SECTOR_HDR_SIZE + FCB_RECORD_HDR_SIZE + (uint32_t)pay_len;
    uint8_t  zero     = 0x00U;
    flash_write(crc_addr, &zero, 1);

    uint8_t buf[64];
    size_t  len_out = 0;
    int     rc      = fcb_read(&fcb, buf, sizeof(buf), &len_out);

    TEST_ASSERT_EQUAL_INT(FCB_EMPTY, rc);
    TEST_ASSERT_EQUAL_UINT32(1, fcb.corrupted_count);

    flash_deinit();
}

void test_fcb_read_crc_corrupted_skips_to_next_valid(void)
{
    flash_init(NULL);

    Fcb       fcb;
    FcbConfig cfg;
    setup_config(&cfg);
    TEST_ASSERT_EQUAL_INT(FCB_OK, fcb_init(&fcb, &cfg));

    const uint8_t payload_a[] = "bad_record";
    const uint8_t payload_b[] = "good_record";
    const size_t  len_a       = sizeof(payload_a);
    const size_t  len_b       = sizeof(payload_b);

    TEST_ASSERT_EQUAL_INT(FCB_OK, fcb_write(&fcb, payload_a, len_a));
    TEST_ASSERT_EQUAL_INT(FCB_OK, fcb_write(&fcb, payload_b, len_b));

    /* Corrupt the CRC of record A.
     * A sits at sector_start + FCB_SECTOR_HDR_SIZE.
     * Its CRC is at: HDR_SIZE + RECORD_HDR_SIZE + len_a */
    uint32_t crc_addr_a = FCB_SECTOR_HDR_SIZE + FCB_RECORD_HDR_SIZE + (uint32_t)len_a;
    uint8_t  zero       = 0x00U;
    flash_write(crc_addr_a, &zero, 1);

    uint8_t buf[64];
    size_t  len_out = 0;
    int     rc      = fcb_read(&fcb, buf, sizeof(buf), &len_out);

    TEST_ASSERT_EQUAL_INT(FCB_OK, rc);
    TEST_ASSERT_EQUAL_size_t(len_b, len_out);
    TEST_ASSERT_EQUAL_MEMORY(payload_b, buf, len_b);
    TEST_ASSERT_EQUAL_UINT32(1, fcb.corrupted_count);

    TEST_ASSERT_EQUAL_INT(FCB_EMPTY, fcb_read(&fcb, buf, sizeof(buf), &len_out));

    flash_deinit();
}

/* ================================================================== */
/*  Section 7: Trim edge cases                                         */
/* ================================================================== */

void test_fcb_trim_on_empty_fcb_returns_ok(void)
{
    flash_init(NULL);

    Fcb       fcb;
    FcbConfig cfg;
    setup_config(&cfg);
    TEST_ASSERT_EQUAL_INT(FCB_OK, fcb_init(&fcb, &cfg));

    TEST_ASSERT_TRUE(fcb_is_empty(&fcb));

    int rc = fcb_trim(&fcb);
    TEST_ASSERT_EQUAL_INT(FCB_OK, rc);

    /* FCB must still be in a usable state after trimming empty buffer */
    TEST_ASSERT_TRUE(fcb.is_mounted);

    flash_deinit();
}

void test_fcb_trim_advances_read_ptr_when_pointing_at_erased_sector(void)
{
    flash_init(NULL);

    Fcb       fcb;
    FcbConfig cfg;
    setup_config(&cfg);
    TEST_ASSERT_EQUAL_INT(FCB_OK, fcb_init(&fcb, &cfg));

    /* Fill sector 0 fully, then write into sector 1 so delete_sector stays at 0. */
    uint8_t payload[FCB_MAX_RECORD_SIZE];
    memset(payload, 0xCCU, sizeof(payload));

    /* Keep writing until the write pointer moves past sector 0. */
    uint32_t writes = 0;
    while (fcb.write_sector == 0)
    {
        int rc = fcb_write(&fcb, payload, sizeof(payload));
        if (rc != FCB_OK)
        {
            break;
        }
        writes++;
        TEST_ASSERT_TRUE_MESSAGE(writes < 1000, "write_sector stuck on 0");
    }

    /* Read all records so read_ptr advances past sector 0 as well. */
    uint8_t buf[FCB_MAX_RECORD_SIZE];
    size_t  len_out;
    while (fcb_read(&fcb, buf, sizeof(buf), &len_out) == FCB_OK)
    {
        /* drain */
    }

    /* At this point read_ptr and delete_ptr are ahead of sector 0.
     * delete_sector should still point to sector 0 (the oldest). */
    uint32_t original_delete_sector = fcb.delete_sector;

    int rc = fcb_trim(&fcb);
    TEST_ASSERT_EQUAL_INT(FCB_OK, rc);

    /* After trim the delete_sector must have advanced. */
    TEST_ASSERT_FALSE_MESSAGE(
        fcb.delete_sector == original_delete_sector,
        "delete_sector did not advance after trim");

    /* FCB must still be in a usable state. */
    TEST_ASSERT_TRUE(fcb.is_mounted);

    flash_deinit();
}

/* ================================================================== */
/*  Section 8: Append lifecycle gaps                                   */
/* ================================================================== */

void test_fcb_append_begin_null_entry_returns_invalid(void)
{
    flash_init(NULL);

    Fcb       fcb;
    FcbConfig cfg;
    setup_config(&cfg);
    TEST_ASSERT_EQUAL_INT(FCB_OK, fcb_init(&fcb, &cfg));

    int rc = fcb_append_begin(&fcb, 16, NULL);
    TEST_ASSERT_EQUAL_INT(FCB_INVALID_ARG, rc);

    /* Lock must have been released — a normal write must succeed. */
    uint8_t data[] = "after_null_entry";
    TEST_ASSERT_EQUAL_INT(FCB_OK, fcb_write(&fcb, data, sizeof(data)));

    flash_deinit();
}

void test_fcb_append_write_zero_len_returns_invalid(void)
{
    flash_init(NULL);

    Fcb       fcb;
    FcbConfig cfg;
    setup_config(&cfg);
    TEST_ASSERT_EQUAL_INT(FCB_OK, fcb_init(&fcb, &cfg));

    FcbEntry entry;
    TEST_ASSERT_EQUAL_INT(FCB_OK, fcb_append_begin(&fcb, 16, &entry));

    uint8_t data[] = "x";
    TEST_ASSERT_EQUAL_INT(FCB_INVALID_ARG, fcb_append_write(&fcb, &entry, data, 0));

    fcb_append_abort(&fcb, &entry);

    flash_deinit();
}

void test_fcb_append_abort_with_no_writes_subsequent_write_ok(void)
{
    flash_init(NULL);

    Fcb       fcb;
    FcbConfig cfg;
    setup_config(&cfg);
    TEST_ASSERT_EQUAL_INT(FCB_OK, fcb_init(&fcb, &cfg));

    /* Begin a 32-byte record, write nothing, then abort. */
    FcbEntry entry;
    TEST_ASSERT_EQUAL_INT(FCB_OK, fcb_append_begin(&fcb, 32, &entry));
    TEST_ASSERT_EQUAL_INT(FCB_OK, fcb_append_abort(&fcb, &entry));

    /* Lock must be released — a subsequent write must succeed. */
    const uint8_t good[] = "after_abort";
    TEST_ASSERT_EQUAL_INT(FCB_OK, fcb_write(&fcb, good, sizeof(good)));

    /* And the aborted record must not surface on read. */
    uint8_t buf[64];
    size_t  len_out = 0;
    TEST_ASSERT_EQUAL_INT(FCB_OK, fcb_read(&fcb, buf, sizeof(buf), &len_out));
    TEST_ASSERT_EQUAL_size_t(sizeof(good), len_out);
    TEST_ASSERT_EQUAL_MEMORY(good, buf, sizeof(good));

    TEST_ASSERT_EQUAL_INT(FCB_EMPTY, fcb_read(&fcb, buf, sizeof(buf), &len_out));

    flash_deinit();
}

void test_fcb_append_double_abort_second_returns_invalid(void)
{
    flash_init(NULL);

    Fcb       fcb;
    FcbConfig cfg;
    setup_config(&cfg);
    TEST_ASSERT_EQUAL_INT(FCB_OK, fcb_init(&fcb, &cfg));

    FcbEntry entry;
    TEST_ASSERT_EQUAL_INT(FCB_OK, fcb_append_begin(&fcb, 8, &entry));

    /* First abort: must succeed and clear in_progress. */
    TEST_ASSERT_EQUAL_INT(FCB_OK, fcb_append_abort(&fcb, &entry));
    TEST_ASSERT_EQUAL_INT_MESSAGE(
        0, entry.in_progress,
        "in_progress must be 0 after first abort");

    /* Second abort on the same (already-closed) entry: must return FCB_INVALID_ARG
     * and must NOT call unlock a second time (which would double-release the mutex). */
    TEST_ASSERT_EQUAL_INT(FCB_INVALID_ARG, fcb_append_abort(&fcb, &entry));

    flash_deinit();
}

/* ================================================================== */
/*  Section 9: Remount persistence                                     */
/* ================================================================== */

void test_fcb_remount_after_delete_skips_consumed_records(void)
{
    flash_init(NULL);

    Fcb       fcb;
    FcbConfig cfg;
    setup_config(&cfg);
    TEST_ASSERT_EQUAL_INT(FCB_OK, fcb_init(&fcb, &cfg));

    const uint8_t rec_a[] = "alpha";
    const uint8_t rec_b[] = "beta";
    const uint8_t rec_c[] = "gamma";

    TEST_ASSERT_EQUAL_INT(FCB_OK, fcb_write(&fcb, rec_a, sizeof(rec_a)));
    TEST_ASSERT_EQUAL_INT(FCB_OK, fcb_write(&fcb, rec_b, sizeof(rec_b)));
    TEST_ASSERT_EQUAL_INT(FCB_OK, fcb_write(&fcb, rec_c, sizeof(rec_c)));

    /* Read and delete the first two records. */
    uint8_t buf[64];
    size_t  len_out;
    TEST_ASSERT_EQUAL_INT(FCB_OK, fcb_read(&fcb, buf, sizeof(buf), &len_out));
    TEST_ASSERT_EQUAL_INT(FCB_OK, fcb_read(&fcb, buf, sizeof(buf), &len_out));
    TEST_ASSERT_EQUAL_INT(FCB_OK, fcb_delete(&fcb));

    /* Write one more record after the delete. */
    const uint8_t rec_d[] = "delta";
    TEST_ASSERT_EQUAL_INT(FCB_OK, fcb_write(&fcb, rec_d, sizeof(rec_d)));

    /* Remount: recovery must skip the consumed A and B, land on C. */
    Fcb       fcb2;
    FcbConfig cfg2;
    setup_config(&cfg2);
    TEST_ASSERT_EQUAL_INT(FCB_OK, fcb_init(&fcb2, &cfg2));

    TEST_ASSERT_EQUAL_INT(FCB_OK, fcb_read(&fcb2, buf, sizeof(buf), &len_out));
    TEST_ASSERT_EQUAL_size_t(sizeof(rec_c), len_out);
    TEST_ASSERT_EQUAL_MEMORY(rec_c, buf, sizeof(rec_c));

    TEST_ASSERT_EQUAL_INT(FCB_OK, fcb_read(&fcb2, buf, sizeof(buf), &len_out));
    TEST_ASSERT_EQUAL_size_t(sizeof(rec_d), len_out);
    TEST_ASSERT_EQUAL_MEMORY(rec_d, buf, sizeof(rec_d));

    TEST_ASSERT_EQUAL_INT(FCB_EMPTY, fcb_read(&fcb2, buf, sizeof(buf), &len_out));

    flash_deinit();
}

/* ================================================================== */
/*  Runner                                                             */
/* ================================================================== */

void run_fcb_ops_tests(void)
{
    /* Config validation */
    RUN_TEST(test_fcb_init_null_fcb_returns_invalid);
    RUN_TEST(test_fcb_init_null_cfg_returns_invalid);
    RUN_TEST(test_fcb_init_zero_sectors_returns_invalid);
    RUN_TEST(test_fcb_init_above_max_sectors_returns_invalid);
    RUN_TEST(test_fcb_init_sector_size_below_minimum_returns_invalid);
    RUN_TEST(test_fcb_init_null_flash_read_returns_invalid);
    RUN_TEST(test_fcb_init_null_flash_write_returns_invalid);
    RUN_TEST(test_fcb_init_null_flash_erase_returns_invalid);

    /* seq_diff contract */
    RUN_TEST(test_fcb_seq_diff_a_newer_than_b);
    RUN_TEST(test_fcb_seq_diff_a_older_than_b);
    RUN_TEST(test_fcb_seq_diff_equal_returns_zero);
    RUN_TEST(test_fcb_seq_diff_wrap_forward);
    RUN_TEST(test_fcb_seq_diff_wrap_backward);

    /* State guard checks */
    RUN_TEST(test_fcb_write_bad_magic_returns_invalid);
    RUN_TEST(test_fcb_delete_null_fcb_returns_invalid);
    RUN_TEST(test_fcb_trim_null_fcb_returns_invalid);
    RUN_TEST(test_fcb_is_full_null_returns_true);
    RUN_TEST(test_fcb_is_empty_null_returns_true);

    /* Minimum payload */
    RUN_TEST(test_fcb_write_one_byte_round_trip);

    /* Flash error propagation */
    RUN_TEST(test_fcb_write_flash_write_error_returns_fcb_err_flash);
    RUN_TEST(test_fcb_trim_flash_erase_error_returns_fcb_err_flash);

    /* CRC corruption */
    RUN_TEST(test_fcb_read_crc_corrupted_returns_empty_increments_count);
    RUN_TEST(test_fcb_read_crc_corrupted_skips_to_next_valid);

    /* Trim edge cases */
    RUN_TEST(test_fcb_trim_on_empty_fcb_returns_ok);
    RUN_TEST(test_fcb_trim_advances_read_ptr_when_pointing_at_erased_sector);

    /* Append lifecycle */
    RUN_TEST(test_fcb_append_begin_null_entry_returns_invalid);
    RUN_TEST(test_fcb_append_write_zero_len_returns_invalid);
    RUN_TEST(test_fcb_append_abort_with_no_writes_subsequent_write_ok);
    RUN_TEST(test_fcb_append_double_abort_second_returns_invalid);

    /* Remount persistence */
    RUN_TEST(test_fcb_remount_after_delete_skips_consumed_records);
}
