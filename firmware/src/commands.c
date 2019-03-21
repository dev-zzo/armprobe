#include <stdint.h>

#include "swd.h"
#include "gpio.h"

void cmd_swd_enable(int enabled)
{
    swd_enable(enabled);
}

void cmd_switch_to_swd(void)
{
    swj_switch_to_swd();
}

int cmd_swd_read(uint8_t cmd_request, void *DataBuffer)
{
    swd_response_t response;
    int parity_ok = 0;

    response = swd_request_response(0x81 | (cmd_request << 1) | (parity_even_4bit(cmd_request) << 5));
    if (response == SWD_RESPONSE_OK) {
        parity_ok = swd_data_read((uint32_t *)DataBuffer);
    }
    swd_turnaround(1);
    swd_idle_cycles();
    
    if (!parity_ok)
        return -1;
    if (response == SWD_RESPONSE_OK)
        return 0;
    return response;
}

int cmd_swd_write(uint8_t cmd_request, const void *DataBuffer)
{
    swd_response_t response;

    response = swd_request_response(0x81 | (cmd_request << 1) | (parity_even_4bit(cmd_request) << 5));
    swd_turnaround(1);
    if (response == SWD_RESPONSE_OK) {
        swd_data_write(*(const uint32_t *)DataBuffer);
    }
    swd_idle_cycles();

    if (response == SWD_RESPONSE_OK)
        return 0;
    return response;
}

void cmd_gpio_configure(int enabled)
{
    gpio_enable(enabled);
}

void cmd_gpio_control(uint8_t bits)
{
    gpio_control(bits);
}
