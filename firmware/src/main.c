/*
 * ARM Debugging Interface (ADI) version 5 prove firmware
 *
 * Board reference:
 *   http://wiki.stm32duino.com/images/a/ae/Bluepillpinout.gif
 */

#include <stm32f10x.h>
#include <stdint.h>

#include "hacks.h"
#include "swd.h"
#include "gpio.h"
#include "usb_core.h"

/* Miscellaneous I/O */

void led_activity(int state)
{
    GPIOC->BSRR = state ? GPIO_BSRR_BR13 : GPIO_BSRR_BS13;
}

/* Commands implementation */

enum {
    CMD_PING,
    CMD_SWJ_CONFIGURE,
    CMD_SWJ_SWITCH_TO_SWD,
    CMD_SWD_READ,
    CMD_SWD_WRITE,
    CMD_GPIO_CONFIGURE,
    CMD_GPIO_CONTROL,
};

enum {
    RSP_SUCCESS,
    RSP_PROTOCOL_ERROR,
    RSP_PARITY_ERROR,
    RSP_BAD_COMMAND,
};

typedef struct {
    uint32_t Tag;
    uint8_t Op;
    uint8_t Param8;
    uint16_t Param16;
    uint32_t Param32;
} ProbeCommandDef;

typedef struct {
    uint32_t Tag;
    uint8_t Status;
    uint8_t SWDResponse;
    uint8_t RESERVED[2];
    uint32_t Data;
} ProbeResponseDef;

volatile ProbeCommandDef Command;
volatile uint8_t CommandAvailable;
ProbeResponseDef Response;

static void ProcessCommand(void)
{
    uint16_t ResponseLength = 5;
    int ResponseStatus = RSP_SUCCESS;

    led_activity(1);
    
    switch (Command.Op) {
    case CMD_PING:
        break;
        
    case CMD_SWJ_CONFIGURE:
        swd_enable(Command.Param8 & 0x01);
        break;
        
    case CMD_SWJ_SWITCH_TO_SWD:
        swj_switch_to_swd();
        break;
        
    case CMD_SWD_READ:
        {
            uint8_t cmd_request = Command.Param8;
            swd_response_t response;
            int parity_ok = 0;

            response = swd_request_response(0x81 | (cmd_request << 1) | (parity_even_4bit(cmd_request) << 5));
            if (response == SWD_RESPONSE_OK) {
                parity_ok = swd_data_read(&Response.Data);
            }
            swd_turnaround(1);
            swd_idle_cycles();
            
            Response.SWDResponse = response;
            if (response == SWD_RESPONSE_OK) {
                if (!parity_ok) {
                    ResponseStatus = RSP_PARITY_ERROR;
                }
                ResponseLength = 12;
            } else {
                ResponseLength = 6;
            }
        }
        break;
        
    case CMD_SWD_WRITE:
        {
            uint8_t cmd_request = Command.Param8;
            swd_response_t response;
            uint32_t value = Command.Param32;

            response = swd_request_response(0x81 | (cmd_request << 1) | (parity_even_4bit(cmd_request) << 5));
            swd_turnaround(1);
            if (response == SWD_RESPONSE_OK) {
                swd_data_write(value);
            }
            swd_idle_cycles();
            
            Response.SWDResponse = response;
            ResponseLength = 6;
        }
        break;

    case CMD_GPIO_CONFIGURE:
        gpio_enable(Command.Param8);
        break;
        
    case CMD_GPIO_CONTROL:
        gpio_control(Command.Param8);
        break;
        
    default:
        Response.Status = RSP_BAD_COMMAND;
        break;
    }
    
    Response.Tag = Command.Tag;
    Response.Status = ResponseStatus;
    
    USB_UserToEndpointMemcpy(2, USB_EP_BUFFER_TX, &Response, ResponseLength);
    USB_SetEPTxStatus(2, USB_EPxR_STAT_TX_VALID);

    led_activity(0);
}

void USB_EP1Handler(USB_EventType Event)
{
    uint16_t Received;
    /* A new command has arrived; store it */
    Received = USB_EndpointToUserMemcpy(1, USB_EP_BUFFER_RX, (void *)&Command, sizeof(Command));
    if (Received != sizeof(Command)) {
        /* Ignore? */
        return;
    }
    CommandAvailable = 1;
    /* This endpoint stays NAK */
}

void USB_EP2Handler(USB_EventType Event)
{
    /* A response has been sent */
    /* This endpoint stays NAK */
    /* Make the command endpoint VALID */
    USB_SetEPRxStatus(1, USB_EPxR_STAT_RX_VALID);
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

    /* Set up I/O Port A: USART1 */
    /* Pin 09: Max 2 MHz, alt out, push-pull */
    /* Pin 10: input, pull-up */
    GPIOA->CRH = (GPIO_CRH_MODE9_1 | GPIO_CRH_CNF9_1) | (GPIO_CRH_CNF10_1);
    GPIOA->ODR |= GPIO_ODR_ODR10;
    /* Set up USART1 */
    RCC->APB2ENR |= RCC_APB2ENR_USART1EN;
    RCC->APB2RSTR &= ~RCC_APB2RSTR_USART1RST;
    USART1->CR1 = USART_CR1_RE | USART_CR1_TE | USART_CR1_UE;
    USART1->CR2 = 0;
    USART1->BRR = 16U * SystemCoreClock / 16U / 1125000U;
    
    USB_Init();
}

int main()
{
    setup();
    for (;;) {
        if (CommandAvailable) {
            CommandAvailable = 0;
            ProcessCommand();
        }
    }
}
