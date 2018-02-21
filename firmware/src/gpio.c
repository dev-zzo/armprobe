#include <stm32f10x.h>
#include <stdint.h>

#include "hacks.h"

/* GPIO pin control implementation */

void gpio_enable(int enabled)
{
    const uint32_t pin_mask = ~(GPIO_CRL_PIN0 | GPIO_CRL_PIN1 | GPIO_CRL_PIN2 | GPIO_CRL_PIN3);
    if (enabled) {
        /* output, push-pull, 2MHz */
        GPIOA->CRL = (GPIOA->CRL & pin_mask) | (GPIO_CRL_MODE0_1 | GPIO_CRL_MODE1_1 | GPIO_CRL_MODE2_1 | GPIO_CRL_MODE3_1);
    } else {
        /* input, floating */
        GPIOA->CRL = (GPIOA->CRL & pin_mask) | (GPIO_CRL_CNF0_0 | GPIO_CRL_CNF1_0 | GPIO_CRL_CNF2_0 | GPIO_CRL_CNF3_0);
    }
}

void gpio_control(uint8_t states)
{
    GPIOA->ODR = (GPIOA->ODR & 0xFFF0) | (states & 0x0F);
}

