/*
 * This file is part of the EasyFlash Library.
 *
 * Copyright (c) 2015-2020, Armink, <armink.ztl@gmail.com>
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
 * Function: It is the configure head file for this library.
 * Created on: 2015-07-14
 */

#ifndef EF_CFG_H_
#define EF_CFG_H_

/* using KVDB feature */
#define EF_USING_KVDB

#ifdef EF_USING_KVDB
/* Auto update KV to latest default when current KVDB version number is changed. @see ef_kvdb.ver_num */
/* #define EF_KV_AUTO_UPDATE */
#endif

/* using TSDB (Time series database) feature */
#define EF_USING_TSDB

/* the flash write granularity, unit: bit
 * only support 1(nor flash)/ 8(stm32f2/f4)/ 32(stm32f1) */
#define EF_WRITE_GRAN                  /* @note you must define it for a value */

/* MCU Endian Configuration, default is Little Endian Order. */
/* #define EF_BIG_ENDIAN  */ 

/* log print macro. default EF_PRINT macro is printf() */
/* #define EF_PRINT(...)               my_printf(__VA_ARGS__) */

/* print debug information */
#define EF_DEBUG_ENABLE

#endif /* EF_CFG_H_ */
