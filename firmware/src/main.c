/*
 * ARM Debugging Interface (ADI) version 5 prove firmware
 *
 * Board reference:
 *   http://wiki.stm32duino.com/images/a/ae/Bluepillpinout.gif
 */

#include <stm32f10x.h>
#include <stdint.h>

/* Hack around to use less defines */
#define GPIO_CRL_PIN0 (GPIO_CRL_CNF0 | GPIO_CRL_MODE0)
#define GPIO_CRL_PIN1 (GPIO_CRL_CNF1 | GPIO_CRL_MODE1)
#define GPIO_CRL_PIN2 (GPIO_CRL_CNF2 | GPIO_CRL_MODE2)
#define GPIO_CRL_PIN3 (GPIO_CRL_CNF3 | GPIO_CRL_MODE3)
#define GPIO_CRL_PIN4 (GPIO_CRL_CNF4 | GPIO_CRL_MODE4)
#define GPIO_CRL_PIN5 (GPIO_CRL_CNF5 | GPIO_CRL_MODE5)
#define GPIO_CRL_PIN6 (GPIO_CRL_CNF6 | GPIO_CRL_MODE6)
#define GPIO_CRL_PIN7 (GPIO_CRL_CNF7 | GPIO_CRL_MODE7)
#define GPIO_CRH_PIN8 (GPIO_CRH_CNF8 | GPIO_CRH_MODE8)
#define GPIO_CRH_PIN9 (GPIO_CRH_CNF9 | GPIO_CRH_MODE9)
#define GPIO_CRH_PIN10 (GPIO_CRH_CNF10 | GPIO_CRH_MODE10)
#define GPIO_CRH_PIN11 (GPIO_CRH_CNF11 | GPIO_CRH_MODE11)
#define GPIO_CRH_PIN12 (GPIO_CRH_CNF12 | GPIO_CRH_MODE12)
#define GPIO_CRH_PIN13 (GPIO_CRH_CNF13 | GPIO_CRH_MODE13)
#define GPIO_CRH_PIN14 (GPIO_CRH_CNF14 | GPIO_CRH_MODE14)
#define GPIO_CRH_PIN15 (GPIO_CRH_CNF15 | GPIO_CRH_MODE15)

/* Miscellaneous I/O */

void led_activity(int state)
{
    GPIOC->BSRR = state ? GPIO_BSRR_BR13 : GPIO_BSRR_BS13;
}

/* Serial Wire Debug Protocol physical layer implementation */

typedef enum _swd_response_t {
    SWD_RESPONSE_OK = 1,
    SWD_RESPONSE_WAIT = 2,
    SWD_RESPONSE_FAULT = 4,
    SWD_PROTOCOL_ERROR = 7,
    SWD_PARITY_ERROR = 8,
} swd_response_t;

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

static void swd_enable(int enabled)
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
    swd_halfbit_delay();
    swd_swclk_L();
    swd_halfbit_delay();
}

/* Rising: bit clock-in; Falling: bit change */
static void swd_bit_outN(int bit)
{
    swd_swclk_L();
    GPIOB->BSRR = bit ? GPIO_BSRR_BS12 : GPIO_BSRR_BR12;
    swd_halfbit_delay();
    swd_swclk_H();
    swd_halfbit_delay();
}

static uint32_t swd_bit_in(void)
{
    uint32_t bit;
    swd_swclk_H();
    swd_halfbit_delay();
    swd_swclk_L();
    bit = (GPIOB->IDR & GPIO_IDR_IDR12) << (31 - 12);
    swd_halfbit_delay();
    return bit;
}

static void swd_turnaround(int writing)
{
    swd_swclk_H();
    GPIOB->BSRR = GPIO_BSRR_BS12;
    if (!writing) {
        /* Input */
        GPIOB->CRH = (GPIOB->CRH & ~GPIO_CRH_PIN12) | (GPIO_CRH_CNF12_1);
    }
    swd_halfbit_delay();
    swd_swclk_L();
    if (writing) {
        /* Output */
        GPIOB->CRH = (GPIOB->CRH & ~GPIO_CRH_PIN12) | (GPIO_CRH_MODE12_0);
    }
    swd_halfbit_delay();
}

static void swd_bits_out(uint32_t bits, int count)
{
    while (count--) {
        swd_bit_out(bits & 1);
        bits >>= 1;
    }
}

static void swd_bits_outN(uint32_t bits, int count)
{
    while (count--) {
        swd_bit_outN(bits & 1);
        bits >>= 1;
    }
}

static uint32_t swd_bits_in(int count)
{
    uint32_t bits = 0;
    int count_in = count;
    while (count_in--) {
        bits = (bits >> 1) | swd_bit_in();
    }
    return bits >> (32 - count);
}

static void swd_idle_cycles(void)
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

static void swj_switch_to_swd(void)
{
    swd_line_reset();
    swd_bits_out(0xE79EU, 16);
    swd_line_reset();
    swd_idle_cycles();
}

static int parity_even_4bit(uint8_t bits)
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

static swd_response_t swd_request_response(uint8_t request)
{
    swd_bits_out(request, 8);
    swd_turnaround(0);
    return (swd_response_t)swd_bits_in(3);
}

static int swd_data_read(uint32_t *value)
{
    uint32_t value_buffer = 0;
    int parity;

    value_buffer = swd_bits_in(32);
    *value = value_buffer;
    parity = swd_bit_in() >> 31;
    return parity == parity_even_32bit(value_buffer);
}

static void swd_data_write(uint32_t value)
{
    int parity;

    parity = parity_even_32bit(value);
    swd_bits_out(value, 32);
    swd_bit_out(parity);
}

/* GPIO pin control implementation */

static void gpio_enable(int enabled)
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

static void gpio_control(uint8_t states)
{
    GPIOA->ODR = (GPIOA->ODR & 0xFFF0) | (states & 0x0F);
}

/* Host interface implementation */

static uint8_t hif_receive_8bit(void)
{
    /* Wait until the next byte is received */
    while (!(USART1->SR & USART_SR_RXNE));
    /* Return it */
    return USART1->DR;
}

static uint32_t hif_receive_32bit(void)
{
    uint32_t value;
    /* Read in the 32-bit value, LSB first */
    value  = hif_receive_8bit();
    value |= hif_receive_8bit() << 8;
    value |= hif_receive_8bit() << 16;
    value |= hif_receive_8bit() << 24;
    
    return value;
}

static void hif_send_8bit(uint8_t value)
{
    /* Wait until the next byte can be sent */
    while (!(USART1->SR & USART_SR_TXE));
    /* Send the next one over */
    USART1->DR = value;
}

static void hif_send_32bit(uint32_t value)
{
    hif_send_8bit((uint8_t)(value >> 0));
    hif_send_8bit((uint8_t)(value >> 8));
    hif_send_8bit((uint8_t)(value >> 16));
    hif_send_8bit((uint8_t)(value >> 24));
}

/* Setting up and main loop */

static void setup(void)
{
    /* Enable GPIOA, GPIOB, GPIOC, AFIO */
    RCC->APB2ENR |= RCC_APB2ENR_IOPCEN | RCC_APB2ENR_IOPBEN | RCC_APB2ENR_IOPAEN | RCC_APB2ENR_AFIOEN;
    RCC->APB2RSTR &= ~(RCC_APB2RSTR_IOPCRST | RCC_APB2RSTR_IOPBRST | RCC_APB2RSTR_IOPARST | RCC_APB2RSTR_AFIORST);

    /* Set up I/O Port A: host IF and GPIO */
    /* Pin 09: Max 2 MHz, alt out, push-pull */
    /* Pin 10: input, pull-up */
    GPIOA->CRH = (GPIO_CRH_MODE9_1 | GPIO_CRH_CNF9_1) | (GPIO_CRH_CNF10_1);
    GPIOA->ODR |= GPIO_ODR_ODR10;
    
    /* Set up I/O port C: LED */
    GPIOC->CRH = (GPIO_CRH_MODE13_1);
    GPIOC->ODR |= GPIO_ODR_ODR13;

    /* Set up USART1 */
    RCC->APB2ENR |= RCC_APB2ENR_USART1EN;
    RCC->APB2RSTR &= ~RCC_APB2RSTR_USART1RST;
    USART1->CR1 = USART_CR1_RE | USART_CR1_TE | USART_CR1_UE;
    USART1->CR2 = 0;
    USART1->BRR = 16U * SystemCoreClock / 16U / 1125000U;
}

static void command_swj_configure(void)
{
    uint8_t cmd_states;

    cmd_states = hif_receive_8bit();

    swd_enable(cmd_states & 0x01);

    hif_send_8bit(0);
}

static void command_swj_read(void)
{
    uint8_t cmd_flags;
    uint8_t cmd_request;
    swd_response_t response;
    uint32_t value;
    int parity_ok = 0;

    cmd_flags = hif_receive_8bit();
    cmd_request = hif_receive_8bit();
    
    led_activity(1);
    
    response = swd_request_response(0x81 | (cmd_request << 1) | (parity_even_4bit(cmd_request) << 5));
    if (response == SWD_RESPONSE_OK) {
        parity_ok = swd_data_read(&value);
    }
    swd_turnaround(1);
    swd_idle_cycles();
    
    hif_send_8bit(0);
    hif_send_8bit(response);
    if (response == SWD_RESPONSE_OK) {
        hif_send_32bit(value);
    }
    
    led_activity(0);
}

static void command_swj_write(void)
{
    uint8_t cmd_flags;
    uint8_t cmd_request;
    swd_response_t response;
    uint32_t value;
    int parity_ok = 0;

    cmd_flags = hif_receive_8bit();
    cmd_request = hif_receive_8bit();
    value = hif_receive_32bit();
    
    led_activity(1);
    
    response = swd_request_response(0x81 | (cmd_request << 1) | (parity_even_4bit(cmd_request) << 5));
    swd_turnaround(1);
    if (response == SWD_RESPONSE_OK) {
        swd_data_write(value);
    }
    swd_idle_cycles();
    
    hif_send_8bit(0);
    hif_send_8bit(response);
    
    led_activity(0);
}

static void command_gpio_configure(void)
{
    uint8_t cmd_config;

    cmd_config = hif_receive_8bit();

    gpio_enable(cmd_config & 0x01);

    hif_send_8bit(0);
}

static void command_gpio_control(void)
{
    uint8_t cmd_states;

    cmd_states = hif_receive_8bit();
    
    gpio_control(cmd_states & 0x0F);

    hif_send_8bit(0);
}

static void command_worker(void)
{
    uint8_t cmd;
    uint8_t param1;
    uint8_t request;
    uint32_t param2;
    swd_response_t response;
    uint32_t result;
    
    cmd = hif_receive_8bit();
    switch (cmd) {
    case 't':
        command_swj_read();
        break;
        
    case 'T':
        command_swj_write();
        break;
        
    case 'c':
        command_swj_configure();
        break;
    
    case 'y':
        swj_switch_to_swd();
        hif_send_8bit(0);
        break;

    case 'G':
        command_gpio_configure();
        break;

    case 'g':
        command_gpio_control();
        break;

    case '?':
        hif_send_8bit('?');
        break;
        
    default:
        hif_send_8bit(0x7F);
        break;
    }
}

void main()
{
    setup();
    for (;;) {
        command_worker();
    }
}
