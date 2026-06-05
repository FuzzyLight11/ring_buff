#include "test_fcb_rw.h"
#include "unity.h"
#include "fcb/fcb.h"
#include "flash_ops/flash_ops.h"
#include "flash_mem/flash_mem.h"
#include <string.h>


/* ================================================================== */
/*  Test function declarations                                         */
/* ================================================================== */

void test_fcb_write_until_full(void);
void test_fcb_read_empty_returns_empty(void);
void test_fcb_rw_fifo_order(void);
void test_fcb_write_invalid_args(void);
void test_fcb_read_invalid_args(void);
void test_fcb_write_max_size_record(void);
void test_fcb_delete_advances_delete_ptr(void);
void test_fcb_delete_nothing_returns_ok(void);
void test_fcb_delete_write_cycle(void);
void test_fcb_trim_frees_oldest_sector(void);
void test_fcb_is_empty_lifecycle(void);

/* ================================================================== */
/*  Tests                                                              */
/* ================================================================== */

void test_fcb_write_until_full(void)
{
    flash_init("../simulation_images/rw_stress.bin");
    flash_full_erase();

    Fcb       fcb;
    FcbConfig cfg;
    setup_config(&cfg);

    int rc = fcb_init(&fcb, &cfg);
    TEST_ASSERT_EQUAL_INT(FCB_OK, rc);

    uint8_t  write_data[] = "StressTestData";
    int      write_rc     = FCB_OK;
    uint32_t write_count  = 0;

    while (1)
    {
        write_rc = fcb_write(&fcb, write_data, sizeof(write_data));
        if (write_rc == FCB_FULL)
        {
            break;
        }
        TEST_ASSERT_EQUAL_INT(FCB_OK, write_rc);
        write_count++;
    }

    TEST_ASSERT_EQUAL_INT(FCB_FULL, write_rc);

    TEST_ASSERT_EQUAL_UINT32(0, fcb.read_sector);
    TEST_ASSERT_EQUAL_UINT32(16, fcb.read_offset);
    TEST_ASSERT_EQUAL_UINT32(0, fcb.delete_sector);
    TEST_ASSERT_EQUAL_UINT32(16, fcb.delete_offset);

    flash_deinit();
}

/* ================================================================== */
/*  test_fcb_read_empty_returns_empty                                  */
/*                                                                     */
/*  Reading from a freshly initialised FCB must return FCB_EMPTY.     */
/* ================================================================== */

void test_fcb_read_empty_returns_empty(void)
{
    flash_init(NULL);

    Fcb       fcb;
    FcbConfig cfg;
    setup_config(&cfg);
    TEST_ASSERT_EQUAL_INT(FCB_OK, fcb_init(&fcb, &cfg));

    uint8_t buf[64];
    size_t  len_out = 0;
    TEST_ASSERT_EQUAL_INT(FCB_EMPTY, fcb_read(&fcb, buf, sizeof(buf), &len_out));
}

/* ================================================================== */
/*  test_fcb_rw_fifo_order                                             */
/*                                                                     */
/*  Three records with distinct payloads are read back in write order. */
/* ================================================================== */

void test_fcb_rw_fifo_order(void)
{
    flash_init(NULL);

    Fcb       fcb;
    FcbConfig cfg;
    setup_config(&cfg);
    TEST_ASSERT_EQUAL_INT(FCB_OK, fcb_init(&fcb, &cfg));

    const uint8_t rec0[] = "alpha";
    const uint8_t rec1[] = "beta-beta";
    const uint8_t rec2[] = "gamma-gamma-gamma";

    TEST_ASSERT_EQUAL_INT(FCB_OK, fcb_write(&fcb, rec0, sizeof(rec0)));
    TEST_ASSERT_EQUAL_INT(FCB_OK, fcb_write(&fcb, rec1, sizeof(rec1)));
    TEST_ASSERT_EQUAL_INT(FCB_OK, fcb_write(&fcb, rec2, sizeof(rec2)));

    uint8_t buf[64];
    size_t  len_out;

    TEST_ASSERT_EQUAL_INT(FCB_OK, fcb_read(&fcb, buf, sizeof(buf), &len_out));
    TEST_ASSERT_EQUAL_size_t(sizeof(rec0), len_out);
    TEST_ASSERT_EQUAL_MEMORY(rec0, buf, sizeof(rec0));

    TEST_ASSERT_EQUAL_INT(FCB_OK, fcb_read(&fcb, buf, sizeof(buf), &len_out));
    TEST_ASSERT_EQUAL_size_t(sizeof(rec1), len_out);
    TEST_ASSERT_EQUAL_MEMORY(rec1, buf, sizeof(rec1));

    TEST_ASSERT_EQUAL_INT(FCB_OK, fcb_read(&fcb, buf, sizeof(buf), &len_out));
    TEST_ASSERT_EQUAL_size_t(sizeof(rec2), len_out);
    TEST_ASSERT_EQUAL_MEMORY(rec2, buf, sizeof(rec2));

    TEST_ASSERT_EQUAL_INT(FCB_EMPTY, fcb_read(&fcb, buf, sizeof(buf), &len_out));
}

/* ================================================================== */
/*  test_fcb_write_invalid_args                                        */
/*                                                                     */
/*  NULL handle, NULL data, zero length, and oversized length each     */
/*  return FCB_INVALID_ARG without touching flash.                     */
/* ================================================================== */

void test_fcb_write_invalid_args(void)
{
    flash_init(NULL);

    Fcb       fcb;
    FcbConfig cfg;
    setup_config(&cfg);
    TEST_ASSERT_EQUAL_INT(FCB_OK, fcb_init(&fcb, &cfg));

    uint8_t data[8];
    memset(data, 0x11, sizeof(data));

    TEST_ASSERT_EQUAL_INT(FCB_INVALID_ARG, fcb_write(NULL, data, sizeof(data)));
    TEST_ASSERT_EQUAL_INT(FCB_INVALID_ARG, fcb_write(&fcb, NULL, sizeof(data)));
    TEST_ASSERT_EQUAL_INT(FCB_INVALID_ARG, fcb_write(&fcb, data, 0));
    TEST_ASSERT_EQUAL_INT(FCB_INVALID_ARG, fcb_write(&fcb, data, FCB_MAX_RECORD_SIZE + 1));
}

/* ================================================================== */
/*  test_fcb_read_invalid_args                                         */
/*                                                                     */
/*  NULL handle, NULL buffer, zero buffer length, and NULL len_out     */
/*  each return FCB_INVALID_ARG.                                       */
/* ================================================================== */

void test_fcb_read_invalid_args(void)
{
    flash_init(NULL);

    Fcb       fcb;
    FcbConfig cfg;
    setup_config(&cfg);
    TEST_ASSERT_EQUAL_INT(FCB_OK, fcb_init(&fcb, &cfg));

    uint8_t data[8];
    memset(data, 0x22, sizeof(data));
    TEST_ASSERT_EQUAL_INT(FCB_OK, fcb_write(&fcb, data, sizeof(data)));

    uint8_t buf[64];
    size_t  len_out;

    TEST_ASSERT_EQUAL_INT(FCB_INVALID_ARG, fcb_read(NULL, buf, sizeof(buf), &len_out));
    TEST_ASSERT_EQUAL_INT(FCB_INVALID_ARG, fcb_read(&fcb, NULL, sizeof(buf), &len_out));
    TEST_ASSERT_EQUAL_INT(FCB_INVALID_ARG, fcb_read(&fcb, buf, 0, &len_out));
    TEST_ASSERT_EQUAL_INT(FCB_INVALID_ARG, fcb_read(&fcb, buf, sizeof(buf), NULL));
}

/* ================================================================== */
/*  test_fcb_write_max_size_record                                     */
/*                                                                     */
/*  A FCB_MAX_RECORD_SIZE-byte payload survives a round-trip          */
/*  without corruption.                                                */
/* ================================================================== */

void test_fcb_write_max_size_record(void)
{
    flash_init(NULL);

    Fcb       fcb;
    FcbConfig cfg;
    setup_config(&cfg);
    TEST_ASSERT_EQUAL_INT(FCB_OK, fcb_init(&fcb, &cfg));

    uint8_t payload[FCB_MAX_RECORD_SIZE];
    int     i;
    for (i = 0; i < (int)FCB_MAX_RECORD_SIZE; i++)
    {
        payload[i] = (uint8_t)(i & 0xFF);
    }

    TEST_ASSERT_EQUAL_INT(FCB_OK, fcb_write(&fcb, payload, FCB_MAX_RECORD_SIZE));

    uint8_t readbuf[FCB_MAX_RECORD_SIZE];
    size_t  len_out = 0;
    TEST_ASSERT_EQUAL_INT(FCB_OK, fcb_read(&fcb, readbuf, FCB_MAX_RECORD_SIZE, &len_out));
    TEST_ASSERT_EQUAL_size_t(FCB_MAX_RECORD_SIZE, len_out);
    TEST_ASSERT_EQUAL_MEMORY(payload, readbuf, FCB_MAX_RECORD_SIZE);
}

/* ================================================================== */
/*  test_fcb_delete_advances_delete_ptr                                */
/*                                                                     */
/*  After reading N records and calling delete, delete_ptr must match  */
/*  read_ptr.                                                          */
/* ================================================================== */

void test_fcb_delete_advances_delete_ptr(void)
{
    flash_init(NULL);

    Fcb       fcb;
    FcbConfig cfg;
    setup_config(&cfg);
    TEST_ASSERT_EQUAL_INT(FCB_OK, fcb_init(&fcb, &cfg));

    uint8_t data[16];
    memset(data, 0x33, sizeof(data));

    TEST_ASSERT_EQUAL_INT(FCB_OK, fcb_write(&fcb, data, sizeof(data)));
    TEST_ASSERT_EQUAL_INT(FCB_OK, fcb_write(&fcb, data, sizeof(data)));
    TEST_ASSERT_EQUAL_INT(FCB_OK, fcb_write(&fcb, data, sizeof(data)));

    uint8_t buf[64];
    size_t  len_out;
    TEST_ASSERT_EQUAL_INT(FCB_OK, fcb_read(&fcb, buf, sizeof(buf), &len_out));
    TEST_ASSERT_EQUAL_INT(FCB_OK, fcb_read(&fcb, buf, sizeof(buf), &len_out));
    TEST_ASSERT_EQUAL_INT(FCB_OK, fcb_read(&fcb, buf, sizeof(buf), &len_out));

    /* Before delete: delete_ptr lags behind read_ptr */
    TEST_ASSERT_FALSE(
        fcb.delete_sector == fcb.read_sector && fcb.delete_offset == fcb.read_offset);

    TEST_ASSERT_EQUAL_INT(FCB_OK, fcb_delete(&fcb));

    /* After delete: delete_ptr must catch up to read_ptr */
    TEST_ASSERT_EQUAL_UINT32(fcb.read_sector, fcb.delete_sector);
    TEST_ASSERT_EQUAL_UINT32(fcb.read_offset, fcb.delete_offset);
}

/* ================================================================== */
/*  test_fcb_delete_nothing_returns_ok                                 */
/*                                                                     */
/*  Calling delete when delete_ptr already equals read_ptr returns     */
/*  FCB_OK and leaves both pointers unchanged.                         */
/* ================================================================== */

void test_fcb_delete_nothing_returns_ok(void)
{
    flash_init(NULL);

    Fcb       fcb;
    FcbConfig cfg;
    setup_config(&cfg);
    TEST_ASSERT_EQUAL_INT(FCB_OK, fcb_init(&fcb, &cfg));

    uint32_t del_sec = fcb.delete_sector;
    uint32_t del_off = fcb.delete_offset;

    TEST_ASSERT_EQUAL_INT(FCB_OK, fcb_delete(&fcb));

    TEST_ASSERT_EQUAL_UINT32(del_sec, fcb.delete_sector);
    TEST_ASSERT_EQUAL_UINT32(del_off, fcb.delete_offset);
}

/* ================================================================== */
/*  test_fcb_delete_write_cycle                                        */
/*                                                                     */
/*  Write 3 records (A/B/C), read them, delete, then write 3 more     */
/*  (D/E/F) and read back.  Verifies that after delete the FCB remains */
/*  in a consistent state and the circular FIFO ordering is preserved. */
/* ================================================================== */

void test_fcb_delete_write_cycle(void)
{
    flash_init(NULL);

    Fcb       fcb;
    FcbConfig cfg;
    setup_config(&cfg);
    TEST_ASSERT_EQUAL_INT(FCB_OK, fcb_init(&fcb, &cfg));

    const uint8_t rec_a[] = "record-A";
    const uint8_t rec_b[] = "record-B";
    const uint8_t rec_c[] = "record-C";

    TEST_ASSERT_EQUAL_INT(FCB_OK, fcb_write(&fcb, rec_a, sizeof(rec_a)));
    TEST_ASSERT_EQUAL_INT(FCB_OK, fcb_write(&fcb, rec_b, sizeof(rec_b)));
    TEST_ASSERT_EQUAL_INT(FCB_OK, fcb_write(&fcb, rec_c, sizeof(rec_c)));

    /* Read all three */
    uint8_t buf[64];
    size_t  len_out;
    TEST_ASSERT_EQUAL_INT(FCB_OK, fcb_read(&fcb, buf, sizeof(buf), &len_out));
    TEST_ASSERT_EQUAL_MEMORY(rec_a, buf, sizeof(rec_a));
    TEST_ASSERT_EQUAL_INT(FCB_OK, fcb_read(&fcb, buf, sizeof(buf), &len_out));
    TEST_ASSERT_EQUAL_MEMORY(rec_b, buf, sizeof(rec_b));
    TEST_ASSERT_EQUAL_INT(FCB_OK, fcb_read(&fcb, buf, sizeof(buf), &len_out));
    TEST_ASSERT_EQUAL_MEMORY(rec_c, buf, sizeof(rec_c));

    /* Delete marks A/B/C consumed and advances delete_ptr to read_ptr */
    TEST_ASSERT_EQUAL_INT(FCB_OK, fcb_delete(&fcb));
    TEST_ASSERT_EQUAL_UINT32(fcb.read_sector, fcb.delete_sector);
    TEST_ASSERT_EQUAL_UINT32(fcb.read_offset, fcb.delete_offset);

    /* Write the next batch and verify FIFO order is preserved */
    const uint8_t rec_d[] = "record-D";
    const uint8_t rec_e[] = "record-E";
    const uint8_t rec_f[] = "record-F";

    TEST_ASSERT_EQUAL_INT(FCB_OK, fcb_write(&fcb, rec_d, sizeof(rec_d)));
    TEST_ASSERT_EQUAL_INT(FCB_OK, fcb_write(&fcb, rec_e, sizeof(rec_e)));
    TEST_ASSERT_EQUAL_INT(FCB_OK, fcb_write(&fcb, rec_f, sizeof(rec_f)));

    TEST_ASSERT_EQUAL_INT(FCB_OK, fcb_read(&fcb, buf, sizeof(buf), &len_out));
    TEST_ASSERT_EQUAL_MEMORY(rec_d, buf, sizeof(rec_d));
    TEST_ASSERT_EQUAL_INT(FCB_OK, fcb_read(&fcb, buf, sizeof(buf), &len_out));
    TEST_ASSERT_EQUAL_MEMORY(rec_e, buf, sizeof(rec_e));
    TEST_ASSERT_EQUAL_INT(FCB_OK, fcb_read(&fcb, buf, sizeof(buf), &len_out));
    TEST_ASSERT_EQUAL_MEMORY(rec_f, buf, sizeof(rec_f));
    TEST_ASSERT_EQUAL_INT(FCB_EMPTY, fcb_read(&fcb, buf, sizeof(buf), &len_out));
}

/* ================================================================== */
/*  test_fcb_trim_frees_oldest_sector                                  */
/*                                                                     */
/*  Fill the FCB (max-size payloads to minimise write count).          */
/*  Verify is_full.  Trim erases the oldest physical sector and        */
/*  advances delete/read pointers.  After trim, is_full must be false  */
/*  and one more write must succeed.                                    */
/* ================================================================== */

void test_fcb_trim_frees_oldest_sector(void)
{
    flash_init(NULL);

    Fcb       fcb;
    FcbConfig cfg;
    setup_config(&cfg);
    TEST_ASSERT_EQUAL_INT(FCB_OK, fcb_init(&fcb, &cfg));

    uint8_t payload[FCB_MAX_RECORD_SIZE];
    memset(payload, 0xBB, sizeof(payload));

    /* Fill until FCB_FULL */
    int write_rc;
    do
    {
        write_rc = fcb_write(&fcb, payload, sizeof(payload));
    } while (write_rc == FCB_OK);

    TEST_ASSERT_EQUAL_INT(FCB_FULL, write_rc);
    TEST_ASSERT_TRUE(fcb_is_full(&fcb));

    /* Trim: erase the oldest sector, advance read/delete pointers */
    TEST_ASSERT_EQUAL_INT(FCB_OK, fcb_trim(&fcb));

    /* Oldest sector gone: FCB is no longer full */
    TEST_ASSERT_FALSE(fcb_is_full(&fcb));

    /* A new write must now succeed */
    TEST_ASSERT_EQUAL_INT(FCB_OK, fcb_write(&fcb, payload, sizeof(payload)));
}

/* ================================================================== */
/*  test_fcb_is_empty_lifecycle                                        */
/*                                                                     */
/*  Fresh FCB → is_empty.  After one write → not empty.               */
/*  After reading that record → is_empty again.                        */
/* ================================================================== */

void test_fcb_is_empty_lifecycle(void)
{
    flash_init(NULL);

    Fcb       fcb;
    FcbConfig cfg;
    setup_config(&cfg);
    TEST_ASSERT_EQUAL_INT(FCB_OK, fcb_init(&fcb, &cfg));

    /* Fresh: must be empty */
    TEST_ASSERT_TRUE(fcb_is_empty(&fcb));
    TEST_ASSERT_FALSE(fcb_is_full(&fcb));

    uint8_t payload[] = "lifecycle";
    TEST_ASSERT_EQUAL_INT(FCB_OK, fcb_write(&fcb, payload, sizeof(payload)));

    /* After write: not empty */
    TEST_ASSERT_FALSE(fcb_is_empty(&fcb));

    uint8_t buf[64];
    size_t  len_out;
    TEST_ASSERT_EQUAL_INT(FCB_OK, fcb_read(&fcb, buf, sizeof(buf), &len_out));

    /* After reading the only record: empty again */
    TEST_ASSERT_TRUE(fcb_is_empty(&fcb));
    TEST_ASSERT_EQUAL_INT(FCB_EMPTY, fcb_read(&fcb, buf, sizeof(buf), &len_out));
}

/* ================================================================== */
/*  Runner                                                             */
/* ================================================================== */

void run_fcb_rw_tests(void)
{
    RUN_TEST(test_fcb_write_until_full);
    RUN_TEST(test_fcb_read_empty_returns_empty);
    RUN_TEST(test_fcb_rw_fifo_order);
    RUN_TEST(test_fcb_write_invalid_args);
    RUN_TEST(test_fcb_read_invalid_args);
    RUN_TEST(test_fcb_write_max_size_record);
    RUN_TEST(test_fcb_delete_advances_delete_ptr);
    RUN_TEST(test_fcb_delete_nothing_returns_ok);
    RUN_TEST(test_fcb_delete_write_cycle);
    RUN_TEST(test_fcb_trim_frees_oldest_sector);
    RUN_TEST(test_fcb_is_empty_lifecycle);
}
