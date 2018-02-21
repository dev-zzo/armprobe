#include <stm32f10x.h>
#include <stdint.h>

#include "hacks.h"
#include "swd.h"

/* Serial Wire Debug Protocol physical layer implementation */

typedef void (*swd_halfbit_delay_t)(void);

static void swd_halfbit_delay(void)
{
    __asm__ __volatile__("\n\tnop;\n\tnop\n\tnop;\n\tnop;"::);
    __asm__ __volatile__("\n\tnop;\n\tnop\n\tnop;\n\tnop;"::);
    __asm__ __volatile__("\n\tnop;\n\tnop\n\tnop;\n\tnop;"::);
    __asm__ __volatile__("\n\tnop;\n\tnop\n\tnop;\n\tnop;"::);
    __asm__ __volatile__("\n\tnop;\n\tnop\n\tnop;\n\tnop;"::);
    __asm__ __volatile__("\n\tnop;\n\tnop\n\tnop;\n\tnop;"::);
    __asm__ __volatile__("\n\tnop;\n\tnop\n\tnop;\n\tnop;"::);
    __asm__ __volatile__("\n\tnop;\n\tnop\n\tnop;\n\tnop;"::);
}

static swd_halfbit_delay_t swd_halfbit_delay_p = swd_halfbit_delay;

void swd_enable(int enabled)
{
    const uint32_t pin_mask = ~(GPIO_CRH_PIN12 | GPIO_CRH_PIN13);
    /* Set up I/O Port B: SWD/JTAG pins */
    if (enabled) {
        /* Pin 12: Max 10 MHz, gpio, push-pull: SWDIO */
        /* Pin 13: Max 10 MHz, gpio, push-pull: SWCLK */
        GPIOB->CRH = (GPIOB->CRH & pin_mask) | (GPIO_CRH_MODE12_0) | (GPIO_CRH_MODE13_0);
        GPIOB->ODR |= GPIO_ODR_ODR12 | GPIO_ODR_ODR13;
    } else {
        /* Pin 12/13: input, floating */
        GPIOB->CRH = (GPIOB->CRH & pin_mask) | (GPIO_CRH_CNF12_0) | (GPIO_CRH_CNF13_0);
    }
}

static void swd_swclk_L(void)
{
    GPIOB->BSRR = GPIO_BSRR_BR13;
}

static void swd_swclk_H(void)
{
    GPIOB->BSRR = GPIO_BSRR_BS13;
}

/* Rising: bit change; Falling: bit clock-in */
static void swd_bit_out(int bit)
{
    swd_swclk_H();
    GPIOB->BSRR = bit ? GPIO_BSRR_BS12 : GPIO_BSRR_BR12;
    swd_halfbit_delay_p();
    swd_swclk_L();
    swd_halfbit_delay_p();
}

#if 0
/* Not used currently */
/* Rising: bit clock-in; Falling: bit change */
static void swd_bit_outN(int bit)
{
    swd_swclk_L();
    GPIOB->BSRR = bit ? GPIO_BSRR_BS12 : GPIO_BSRR_BR12;
    swd_halfbit_delay_p();
    swd_swclk_H();
    swd_halfbit_delay_p();
}
#endif

static uint32_t swd_bit_in(void)
{
    uint32_t bit;
    swd_swclk_H();
    swd_halfbit_delay_p();
    swd_swclk_L();
    bit = (GPIOB->IDR & GPIO_IDR_IDR12) << (31 - 12);
    swd_halfbit_delay_p();
    return bit;
}

void swd_turnaround(int writing)
{
    swd_swclk_H();
    GPIOB->BSRR = GPIO_BSRR_BS12;
    if (!writing) {
        /* Input */
        GPIOB->CRH = (GPIOB->CRH & ~GPIO_CRH_PIN12) | (GPIO_CRH_CNF12_1);
    }
    swd_halfbit_delay_p();
    swd_swclk_L();
    if (writing) {
        /* Output */
        GPIOB->CRH = (GPIOB->CRH & ~GPIO_CRH_PIN12) | (GPIO_CRH_MODE12_0);
    }
    swd_halfbit_delay_p();
}

static void swd_bits_out(uint32_t bits, int count)
{
    while (count--) {
        swd_bit_out(bits & 1);
        bits >>= 1;
    }
}

#if 0
/* Not used currently */
static void swd_bits_outN(uint32_t bits, int count)
{
    while (count--) {
        swd_bit_outN(bits & 1);
        bits >>= 1;
    }
}
#endif

static uint32_t swd_bits_in(int count)
{
    uint32_t bits = 0;
    int count_in = count;
    while (count_in--) {
        bits = (bits >> 1) | swd_bit_in();
    }
    return bits >> (32 - count);
}

void swd_idle_cycles(void)
{
    swd_bits_out(0, 8+1);
}

static void swd_line_reset(void)
{
    int counter;
    
    for (counter = 0; counter < 50; ++counter) {
        swd_swclk_H();
        swd_halfbit_delay();
        swd_swclk_L();
        swd_halfbit_delay();
    }
}

void swj_switch_to_swd(void)
{
    swd_line_reset();
    swd_bits_out(0xE79EU, 16);
    swd_line_reset();
    swd_idle_cycles();
}

int parity_even_4bit(uint8_t bits)
{
    return (0x6996U >> bits) & 1;
}

static int parity_even_32bit(uint32_t bits)
{
    int parity;

    for (parity = 0; bits; bits >>= 4) {
        parity ^= parity_even_4bit(bits & 0xF);
    }

    return parity;
}

swd_response_t swd_request_response(uint8_t request)
{
    swd_bits_out(request, 8);
    swd_turnaround(0);
    return (swd_response_t)swd_bits_in(3);
}

int swd_data_read(uint32_t *value)
{
    uint32_t value_buffer = 0;
    int parity;

    value_buffer = swd_bits_in(32);
    *value = value_buffer;
    parity = swd_bit_in() >> 31;
    return parity == parity_even_32bit(value_buffer);
}

void swd_data_write(uint32_t value)
{
    int parity;

    parity = parity_even_32bit(value);
    swd_bits_out(value, 32);
    swd_bit_out(parity);
}
