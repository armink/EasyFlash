/*
 * This file is part of the EasyFlash Library.
 *
 * Copyright (c) 2015, Armink, <armink.ztl@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * 'Software'), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED 'AS IS', WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * Function: It's a demo for EasyFlash Log fucnction. You must use EasyLogger(https://github.com/armink/EasyLogger).
 * Created on: 2015-07-06
 */

#include "elog_flash.h"

#define log_a(...) elog_a("main.test.a", __VA_ARGS__)
#define log_e(...) elog_e("main.test.e", __VA_ARGS__)
#define log_w(...) elog_w("main.test.w", __VA_ARGS__)
#define log_i(...) elog_i("main.test.i", __VA_ARGS__)
#define log_d(...) elog_d("main.test.d", __VA_ARGS__)
#define log_v(...) elog_v("main.test.v", __VA_ARGS__)

static void test_elog(void);
static void elog_user_assert_hook(const char* ex, const char* func, size_t line);

int main(void){

    /* initialize EasyFlash and EasyLogger */
    if ((easyflash_init() == EF_NO_ERR)&&(elog_init() == ELOG_NO_ERR)) {
        /* set EasyLogger log format */
        elog_set_fmt( ELOG_FMT_LVL | ELOG_FMT_TAG | ELOG_FMT_TIME /*| ELOG_FMT_P_INFO*/ | ELOG_FMT_T_INFO
                            | ELOG_FMT_DIR /*| ELOG_FMT_FUNC*/| ELOG_FMT_LINE);
        elog_set_filter_lvl(ELOG_LVL_VERBOSE);
        /* initialize EasyLogger Flash plugin */
        elog_flash_init();
        /* start EasyLogger */
        elog_start();
        /* set EasyLogger assert hook */
        elog_assert_set_hook(elog_user_assert_hook);
        /* test logger output */
        test_elog();
    }

    return 0;
}

/**
 * Elog demo
 */
static void test_elog(void) {
    /* output all saved log on flash */
    elog_flash_outout_all();
    /* test log output for all level */
    log_a("Hello EasyLogger!");
    log_e("Hello EasyLogger!");
    log_w("Hello EasyLogger!");
    log_i("Hello EasyLogger!");
    log_d("Hello EasyLogger!");
    log_v("Hello EasyLogger!");
    elog_raw("Hello EasyLogger!");
    /* trigger assert. Now will run elog_user_assert_hook. All log information will save to flash. */
    ELOG_ASSERT(0);
}

static void elog_user_assert_hook(const char* ex, const char* func, size_t line) {
    /* disable logger output lock */
    elog_output_lock_enabled(false);
    /* disable flash plugin lock */
    elog_flash_lock_enabled(false);
    /* output assert information */
    elog_a("elog", "(%s) has assert failed at %s:%ld.\n", ex, func, line);
    /* write all buffered log to flash */
    elog_flash_flush();
    while(1);
}