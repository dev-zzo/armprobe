#include <stm32f10x.h>
#include <stdint.h>

#include "debug.h"

/* Debug interface implementation */

static void USART1_Send(uint8_t value)
{
    /* Wait until the next byte can be sent */
    while (!(USART1->SR & USART_SR_TXE));
    /* Send the next one over */
    USART1->DR = value;
}

void DEBUG_PrintString_(const char *s)
{
    while (*s) {
        USART1_Send(*s);
        s++;
    }
}

static const char hextab[] = "0123456789ABCDEF";
static void DEBUG_PrintHexByte(uint8_t value)
{
    USART1_Send(hextab[value >> 4]);
    USART1_Send(hextab[value & 15]);
}

void DEBUG_PrintHex_(const void *p, unsigned n)
{
    const uint8_t *pp = (const uint8_t *)p;
    while (n--) {
        DEBUG_PrintHexByte(*pp);
        pp++;
    }
}

void DEBUG_PrintU16_(uint16_t value)
{
    DEBUG_PrintHexByte((value >> 8) & 0xFF);
    DEBUG_PrintHexByte((value >> 0) & 0xFF);
}

void DEBUG_PrintU32_(uint32_t value)
{
    DEBUG_PrintHexByte((value >> 24) & 0xFF);
    DEBUG_PrintHexByte((value >> 16) & 0xFF);
    DEBUG_PrintHexByte((value >> 8) & 0xFF);
    DEBUG_PrintHexByte((value >> 0) & 0xFF);
}
