#include "bsp.h"
#include "easyflash.h"
#include <stdio.h>
#include <stdlib.h>

static void test_env(void);

int main(void){
        
    BSP_Init();
    
    if (easyflash_init() == EF_NO_ERR) {
        /* test Env demo */
        test_env();
    } 

    while(1) {
      LED_RUN_ON;
      delay(1000000);
      LED_RUN_OFF;
      delay(1000000);      
    }
    
    return 0;
}

/**
 * Env demo.
 */
static void test_env(void) {
    uint32_t i_boot_times = NULL;
    char *c_old_boot_times, c_new_boot_times[11] = {0};

    /* get the boot count number from Env */
    c_old_boot_times = ef_get_env("boot_times");
    assert_param(c_old_boot_times);
    i_boot_times = atol(c_old_boot_times);
    /* boot count +1 */
    i_boot_times ++;
    printf("The system now boot %d times\n\r", i_boot_times);
    /* interger to string */
    sprintf(c_new_boot_times,"%ld", i_boot_times);
    /* set and store the boot count number to Env */
    ef_set_env("boot_times", c_new_boot_times);
    ef_save_env();
}
