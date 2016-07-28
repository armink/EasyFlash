/*
 * This file is part of the Serial Flash Universal Driver Library.
 *
 * Copyright (c) 2016, Armink, <armink.ztl@gmail.com>
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
 * Function: Portable interface for each platform.
 * Created on: 2016-04-23
 */

#include <sfud.h>
#include <stdarg.h>
#include <stdio.h>
#include <stm32f10x_conf.h>

typedef struct {
    SPI_TypeDef *spix;
    GPIO_TypeDef *cs_gpiox;
    uint16_t cs_gpio_pin;
} spi_user_data, *spi_user_data_t;

static char log_buf[256];

void sfud_log_debug(const char *file, const long line, const char *format, ...);

static void rcc_configuration(spi_user_data_t spi) {
    if (spi->spix == SPI1) {
        RCC_APB2PeriphClockCmd(RCC_APB2Periph_SPI1, ENABLE);
        RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
        RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);
    } else if (spi->spix == SPI2) {
        /* you can add SPI2 code here */
    }
}

static void gpio_configuration(spi_user_data_t spi) {
    GPIO_InitTypeDef GPIO_InitStructure;

    if (spi->spix == SPI1) {
        /* SCK:PA5  MISO:PA6  MOSI:PA7 */
        GPIO_InitStructure.GPIO_Pin = GPIO_Pin_5 | GPIO_Pin_6 | GPIO_Pin_7;
        GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
        GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
        GPIO_Init(GPIOA, &GPIO_InitStructure);
        /* CS: PC4 */
        GPIO_InitStructure.GPIO_Pin = GPIO_Pin_4;
        GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
        GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
        GPIO_Init(GPIOC, &GPIO_InitStructure);
        GPIO_SetBits(GPIOC, GPIO_Pin_4);
    } else if (spi->spix == SPI2) {
        /* you can add SPI2 code here */
    }
}

static void spi_configuration(spi_user_data_t spi) {
    SPI_InitTypeDef SPI_InitStructure;

    SPI_InitStructure.SPI_Direction = SPI_Direction_2Lines_FullDuplex; //SPI 设置为双线双向全双工
    SPI_InitStructure.SPI_Mode = SPI_Mode_Master;                      //设置为主 SPI
    SPI_InitStructure.SPI_DataSize = SPI_DataSize_8b;                  //SPI 发送接收 8 位帧结构
    SPI_InitStructure.SPI_CPOL = SPI_CPOL_Low;                         //时钟悬空低
    SPI_InitStructure.SPI_CPHA = SPI_CPHA_1Edge;                       //数据捕获于第一个时钟沿
    //TODO 以后可以尝试硬件 CS
    SPI_InitStructure.SPI_NSS = SPI_NSS_Soft;                          //内部  NSS 信号由 SSI 位控制
    SPI_InitStructure.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_2; //波特率预分频值为 2
    SPI_InitStructure.SPI_FirstBit = SPI_FirstBit_MSB;                 //数据传输从 MSB 位开始
    SPI_InitStructure.SPI_CRCPolynomial = 7;                           // CRC 值计算的多项式

    SPI_I2S_DeInit(spi->spix);
    SPI_Init(spi->spix, &SPI_InitStructure);

    SPI_CalculateCRC(spi->spix, DISABLE);
    SPI_Cmd(spi->spix, ENABLE);
}

static void spi_lock(const sfud_spi *spi) {
    __disable_irq();
}

static void spi_unlock(const sfud_spi *spi) {
    __enable_irq();
}

/**
 * SPI write data then read data
 */
static sfud_err spi_write_read(const sfud_spi *spi, const uint8_t *write_buf, size_t write_size, uint8_t *read_buf,
        size_t read_size) {
    sfud_err result = SFUD_SUCCESS;
    uint8_t send_data, read_data;
    spi_user_data_t spi_dev = (spi_user_data_t) spi->user_data;

    if (write_size) {
        SFUD_ASSERT(write_buf);
    }
    if (read_size) {
        SFUD_ASSERT(read_buf);
    }

    GPIO_ResetBits(spi_dev->cs_gpiox, spi_dev->cs_gpio_pin);
    /* 开始读写数据 */
    for (size_t i = 0, retry_times; i < write_size + read_size; i++) {
        /* 先写缓冲区中的数据到 SPI 总线，数据写完后，再写 dummy(0xFF) 到 SPI 总线 */
        if (i < write_size) {
            send_data = *write_buf++;
        } else {
            send_data = SFUD_DUMMY_DATA;
        }
        /* 发送数据 */
        retry_times = 1000;
        while (SPI_I2S_GetFlagStatus(spi_dev->spix, SPI_I2S_FLAG_TXE) == RESET) {
            SFUD_RETRY_PROCESS(NULL, retry_times, result);
        }
        if (result != SFUD_SUCCESS) {
            goto exit;
        }
        SPI_I2S_SendData(spi_dev->spix, send_data);
        /* 接收数据 */
        retry_times = 1000;
        while (SPI_I2S_GetFlagStatus(spi_dev->spix, SPI_I2S_FLAG_RXNE) == RESET) {
            SFUD_RETRY_PROCESS(NULL, retry_times, result);
        }
        if (result != SFUD_SUCCESS) {
            goto exit;
        }
        read_data = SPI_I2S_ReceiveData(spi_dev->spix);
        /* 写缓冲区中的数据发完后，再读取 SPI 总线中的数据到读缓冲区 */
        if (i >= write_size) {
            *read_buf++ = read_data;
        }
    }

exit:
    GPIO_SetBits(spi_dev->cs_gpiox, spi_dev->cs_gpio_pin);

    return result;
}

/* about 100 microsecond delay */
static void retry_delay_100us(void) {
    uint32_t delay = 120;
    while(delay--);
}

static spi_user_data spi1 = { .spix = SPI1, .cs_gpiox = GPIOC, .cs_gpio_pin = GPIO_Pin_4 };
sfud_err sfud_spi_port_init(sfud_flash *flash) {
    sfud_err result = SFUD_SUCCESS;

    switch (flash->index) {
    case SFUD_SST25_DEVICE_INDEX: {
        /* RCC 初始化 */
        rcc_configuration(&spi1);
        /* GPIO 初始化 */
        gpio_configuration(&spi1);
        /* SPI 外设初始化 */
        spi_configuration(&spi1);
        /* 同步 Flash 移植所需的接口及数据 */
        flash->spi.wr = spi_write_read;
        flash->spi.lock = spi_lock;
        flash->spi.unlock = spi_unlock;
        flash->spi.user_data = &spi1;
        /* about 100 microsecond delay */
        flash->retry.delay = retry_delay_100us;
        /* adout 60 seconds timeout */
        flash->retry.times = 60 * 10000;

        break;
    }
    }

    return result;
}

/**
 * This function is print debug info.
 *
 * @param file the file which has call this function
 * @param line the line number which has call this function
 * @param format output format
 * @param ... args
 */
void sfud_log_debug(const char *file, const long line, const char *format, ...) {
    va_list args;

    /* args point to the first variable parameter */
    va_start(args, format);
    printf("[SFUD](%s:%ld) ", file, line);
    /* must use vprintf to print */
    vsnprintf(log_buf, sizeof(log_buf), format, args);
    printf("%s\r\n", log_buf);
    va_end(args);
}

/**
 * This function is print routine info.
 *
 * @param format output format
 * @param ... args
 */
void sfud_log_info(const char *format, ...) {
    va_list args;

    /* args point to the first variable parameter */
    va_start(args, format);
    printf("[SFUD]");
    /* must use vprintf to print */
    vsnprintf(log_buf, sizeof(log_buf), format, args);
    printf("%s\r\n", log_buf);
    va_end(args);
}
