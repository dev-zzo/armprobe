#include <stm32f10x.h>
#include <stdint.h>

#include "debug.h"

#define BUFFER_SIZE 1024

static char Buffer[BUFFER_SIZE];
static unsigned ReadIndex;
static unsigned WriteIndex;

/* Debug interface implementation */

void DEBUG_Init(void)
{
    /* Set up I/O Port A: USART1 */
    /* Pin 09: Max 2 MHz, alt out, push-pull */
    /* Pin 10: input, pull-up */
    GPIOA->CRH = (GPIO_CRH_MODE9_1 | GPIO_CRH_CNF9_1) | (GPIO_CRH_CNF10_1);
    GPIOA->ODR |= GPIO_ODR_ODR10;
    /* Set up USART1 */
    RCC->APB2ENR |= RCC_APB2ENR_USART1EN;
    RCC->APB2RSTR &= ~RCC_APB2RSTR_USART1RST;
    USART1->CR1 = USART_CR1_TE | USART_CR1_UE;
    USART1->CR2 = 0;
    //USART1->BRR = 16U * SystemCoreClock / 16U / 1125000U;
    USART1->BRR = 16U * SystemCoreClock / 16U / 115200U;
    ReadIndex = 0;
    WriteIndex = 0;
    /* Configure interrupts */
    NVIC_SetPriority(USART1_IRQn, 0x30);
    NVIC_EnableIRQ(USART1_IRQn);
}

static void DEBUG_Put(char x)
{
    unsigned new_index = (WriteIndex + 1) & (BUFFER_SIZE - 1);
    /* Store the value */
    Buffer[new_index] = x;
    WriteIndex = new_index;
}

static void DEBUG_Commit(void)
{
    /* Enable interrupt */
    USART1->CR1 |= USART_CR1_TXEIE;
}

void USART1_IRQHandler(void)
{
    uint16_t status = USART1->SR;
    if (status & USART_SR_TXE) {
        if (GPIOA->IDR & GPIO_IDR_IDR10) {
            unsigned new_index = (ReadIndex + 1) & (BUFFER_SIZE - 1);
            USART1->DR = Buffer[new_index];
            ReadIndex = new_index;
            if (new_index != WriteIndex) {
                USART1->CR1 |= USART_CR1_TXEIE;
            }
            else {
                USART1->CR1 &= ~USART_CR1_TXEIE;
            }
        }
    }
}

void DEBUG_PrintChar_(char x)
{
    DEBUG_Put(x);
    DEBUG_Commit();
}

void DEBUG_PrintString_(const char *s)
{
    while (*s) {
        DEBUG_Put(*s);
        s++;
    }
    DEBUG_Commit();
}

static const char hextab[] = "0123456789ABCDEF";
static void DEBUG_PrintHexByte(uint8_t value)
{
    DEBUG_Put(hextab[value >> 4]);
    DEBUG_Put(hextab[value & 15]);
}

void DEBUG_PrintHex_(const void *p, unsigned n)
{
    const uint8_t *pp = (const uint8_t *)p;
    while (n--) {
        DEBUG_PrintHexByte(*pp);
        pp++;
        DEBUG_PrintChar_(' ');
    }
    DEBUG_Commit();
}

void DEBUG_PrintU8_(uint8_t value)
{
    DEBUG_PrintHexByte(value);
    DEBUG_Commit();
}

void DEBUG_PrintU16_(uint16_t value)
{
    DEBUG_PrintHexByte((value >> 8) & 0xFF);
    DEBUG_PrintHexByte((value >> 0) & 0xFF);
    DEBUG_Commit();
}

void DEBUG_PrintU32_(uint32_t value)
{
    DEBUG_PrintHexByte((value >> 24) & 0xFF);
    DEBUG_PrintHexByte((value >> 16) & 0xFF);
    DEBUG_PrintHexByte((value >> 8) & 0xFF);
    DEBUG_PrintHexByte((value >> 0) & 0xFF);
    DEBUG_Commit();
}
