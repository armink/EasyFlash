/*
 * user_finsh_cmd.c
 *
 *  Created on: 2013Äê12ÔÂ7ÈÕ
 *      Author: Armink
 */
#include <rthw.h>
#include <rtthread.h>
#include <stm32f10x_conf.h>
#include "easyflash.h"
#include "finsh.h"

void reboot(uint8_t argc, char **argv) {
    NVIC_SystemReset();
}
MSH_CMD_EXPORT(reboot, Reboot System);

void get_cpuusage(void) {
    extern uint8_t cpu_usage_major, cpu_usage_minor;
    rt_kprintf("The CPU usage is %d.%d% now.\n", cpu_usage_major, cpu_usage_minor);
}
MSH_CMD_EXPORT(get_cpuusage, Get control board cpu usage);
