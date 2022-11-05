/*
 * Copyright (c) The Libre Solar Project Contributors
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef UNIT_TEST

#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>


#include <stdio.h>

#include "bms.h"
#include "button.h"
#include "data_objects.h"
#include "eeprom.h"
#include "helper.h"
#include "leds.h"
#include "thingset.h"

LOG_MODULE_REGISTER(bms_main, CONFIG_LOG_DEFAULT_LEVEL);

Bms bms;

extern ThingSet ts;


static const struct gpio_dt_spec led_chg = GPIO_DT_SPEC_GET(DT_ALIAS(led_green), gpios);


int main(void)
{
    printf("Hardware: Libre Solar %s (%s)\n", DT_PROP(DT_PATH(pcb), type),
           DT_PROP(DT_PATH(pcb), version_str));
    printf("Firmware: %s\n", FIRMWARE_VERSION_ID);

    bms_init_status(&bms);
    bms_init_config(&bms, CONFIG_CELL_TYPE, CONFIG_BAT_CAPACITY_AH);

    // read custom configuration from EEPROM
    data_objects_init();

    while (bms_init_hardware(&bms) != 0) {
        LOG_ERR("BMS hardware initialization failed, retrying in 10s");
        k_sleep(K_MSEC(10000));
    }


    if (!device_is_ready(led_chg.port)) {
        return 0;
    }


    float x = bms.conf.cell_ov_limit * 2000;
    printf("%f\n", x);

    bms_apply_cell_ovp(&bms);
    bms_apply_cell_uvp(&bms);

    bms_apply_dis_scp(&bms);
    bms_apply_dis_ocp(&bms);
    bms_apply_chg_ocp(&bms);

    bms_apply_temp_limits(&bms);
    bms_apply_balancing_conf(&bms);

    bms_update(&bms);
    bms_soc_reset(&bms, -1);

    button_init();

    int64_t t_start = k_uptime_get();
    while (true) {

        bms_update(&bms);
        bms_state_machine(&bms);

        if (button_pressed_for_3s()) {
            printf("Button pressed for 3s: shutdown...\n");
            bms_shutdown(&bms);
            k_sleep(K_MSEC(10000));
        }

//         bms_print_registers();
        printf("Connected cells: %d\n", bms.status.connected_cells);

        for(int i =0;i<BOARD_NUM_CELLS_MAX;i++){
            printf("%d: %fV\n", i, bms.status.cell_voltages[i]);
        }

        printf("Pack voltage: %f\n", bms.status.pack_voltage);
        printf("Current: %f\n", bms.status.pack_current);
        printf("Flags %x\n", bms.status.error_flags);
        printf("temp_min: %f temp_max: %f\n", bms.status.bat_temp_min, bms.status.bat_temp_max);

        eeprom_update();

        t_start += 100;
        k_sleep(K_TIMEOUT_ABS_MS(t_start));
    }

    return 0;
}

#endif /* UNIT_TEST */
