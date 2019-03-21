/*
 * ARM Debugging Interface (ADI) version 5 probe firmware
 *
 * Board reference:
 *   http://wiki.stm32duino.com/images/a/ae/Bluepillpinout.gif
 */

#include <stm32f10x.h>
#include <stdint.h>

#include "hacks.h"
#include "debug.h"
#include "usb_core.h"

/* Miscellaneous I/O */

void led_activity(int state)
{
    GPIOC->BSRR = state ? GPIO_BSRR_BR13 : GPIO_BSRR_BS13;
}

/* Setting up and main loop */

static void setup(void)
{
    /* Enable GPIOA, GPIOB, GPIOC, AFIO */
    RCC->APB2ENR |= RCC_APB2ENR_IOPCEN | RCC_APB2ENR_IOPBEN | RCC_APB2ENR_IOPAEN | RCC_APB2ENR_AFIOEN;
    RCC->APB2RSTR &= ~(RCC_APB2RSTR_IOPCRST | RCC_APB2RSTR_IOPBRST | RCC_APB2RSTR_IOPARST | RCC_APB2RSTR_AFIORST);
    
    /* Set up I/O port C: LED */
    GPIOC->CRH = (GPIO_CRH_MODE13_1);
    GPIOC->ODR |= GPIO_ODR_ODR13;

    DEBUG_Init();
    USB_Init();
}

int main()
{
    setup();
    for (;;) {
    }
}
