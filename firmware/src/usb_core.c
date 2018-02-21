#include "usb_core.h"

/******************************************************************************/
/* Hardware abstraction and support code                                      */
/******************************************************************************/

void USB_Enable(int Enabled)
{
    /* Force reset */
    USB->CNTR = USB_CNTR_FRES;
    /* Clear all pending ints */
    USB->ISTR = 0;
    if (Enabled) {
        /* Enable RESET int */
        USB->CNTR = USB_CNTR_RESETM;
    } else {
        /* Switch off the peripheral */
        USB->CNTR = USB_CNTR_FRES | USB_CNTR_PDWN;
    }
}

void USB_Init()
{
    /* Enable clock */
    RCC->APB1ENR |= RCC_APB1ENR_USBEN;
    RCC->APB1RSTR &= ~RCC_APB1RSTR_USBRST;
    /* Configure interrupts */
    NVIC->IP[USB_LP_CAN1_RX0_IRQn] = 0x10;
    NVIC->ISER[0] |= (1 << USB_LP_CAN1_RX0_IRQn);
    /* Enable the RESET interrupt */
    USB_Enable(TRUE);
}

/******************************************************************************/
/* USB PMA related routines                                                   */
/******************************************************************************/

void USB_ConfigureTxBuffer(unsigned EPIndex, unsigned BufferIndex, uint16_t Address)
{
    USB_TxDescriptor *Descr = USB_GetTxDescriptor(EPIndex, BufferIndex);
    Descr->ADDR_TX = Address;
    Descr->COUNT_TX = 0;
}

void USB_ConfigureRxBuffer(unsigned EPIndex, unsigned BufferIndex, uint16_t Address, uint16_t Size)
{
    USB_RxDescriptor *Descr = USB_GetRxDescriptor(EPIndex, BufferIndex);
    uint16_t Blocks;
    Descr->ADDR_RX = Address;
    if (Size > 62) {
        /* Use 32-byte blocks */
        Descr->BLOCK_SIZE = 1;
        Blocks = (Size + 31) >> 5;
    } else {
        /* Use 2-byte blocks */
        Descr->BLOCK_SIZE = 0;
        Blocks = (Size + 1) >> 1;
    }
    Descr->NUM_BLOCK = Blocks;
}

static void USB_UserToPMAMemcpy(const void *UserBuffer, uint16_t PMAAddress, uint16_t Count)
{
    const uint16_t *UserBufferPtr = (const uint16_t *)UserBuffer;
    uint32_t n = (Count + 1) >> 1;   /* n = (Count + 1) / 2 */
    uint16_t *PMABufferPtr = (uint16_t *)(PMA_BASE + PMAAddress * 2);
    
    while (n--) {
        *PMABufferPtr++ = *UserBufferPtr++;
        PMABufferPtr++;
    }
}

static void USB_PMAToUserMemcpy(void *UserBuffer, uint16_t PMAAddress, uint16_t Count)
{
    uint16_t *UserBufferPtr = (uint16_t *)UserBuffer;
    uint32_t n = (Count + 1) >> 1;   /* n = (Count + 1) / 2 */
    uint16_t *PMABufferPtr = (uint16_t *)(PMA_BASE + PMAAddress * 2);
    
    while (n--) {
        *UserBufferPtr++ = *PMABufferPtr++;
        PMABufferPtr++;
    }
}

void USB_UserToEndpointMemcpy(unsigned EPIndex, unsigned BufferIndex, const void *UserBuffer, uint16_t DataSize)
{
    USB_TxDescriptor *Descr = USB_GetTxDescriptor(EPIndex, BufferIndex);
    /* Copy data */
    USB_UserToPMAMemcpy(UserBuffer, Descr->ADDR_TX, DataSize);
    /* Update the count */
    Descr->COUNT_TX = DataSize;
}

uint16_t USB_EndpointToUserMemcpy(unsigned EPIndex, unsigned BufferIndex, void *UserBuffer, uint16_t BufferSize)
{
    USB_RxDescriptor *Descr = USB_GetRxDescriptor(EPIndex, BufferIndex);
    uint16_t DataSize = Descr->COUNT_RX;
    /* Make sure we don't overflow the buffer */
    if (DataSize > BufferSize) {
        DataSize = BufferSize;
    }
    /* Copy data */
    USB_PMAToUserMemcpy(UserBuffer, Descr->ADDR_RX, DataSize);
    /* Let the user know how much was copied */
    return DataSize;
}

/******************************************************************************/
/* USB ISR handling routines                                                  */
/******************************************************************************/

void USB_EP0Configure(void);

static void USB_ResetHandler(void)
{
    /* Clear ints */
    USB->ISTR = 0;
    /* Setup intr mask */
    USB->CNTR = USB_CNTR_CTRM | USB_CNTR_RESETM | USB_HANDLED_INTS;
    /* Configure endpoint buffers */
    USB->BTABLE = USB_BTABLE;
    /* Configure endpoint 0 */
    USB_EP0Configure();
    /* Enable function */
    USB->DADDR = USB_DADDR_EF;
}

typedef void (*USB_EventHandlerDef)(USB_EventType Event);

void USB_EPxNullHandler(USB_EventType Event)
{
    /* Nothing */
}

void USB_EP0Handler(USB_EventType Event);
void USB_EP1Handler(USB_EventType Event) __attribute__((weak, alias("USB_EPxNullHandler")));
void USB_EP2Handler(USB_EventType Event) __attribute__((weak, alias("USB_EPxNullHandler")));
void USB_EP3Handler(USB_EventType Event) __attribute__((weak, alias("USB_EPxNullHandler")));
void USB_EP4Handler(USB_EventType Event) __attribute__((weak, alias("USB_EPxNullHandler")));
void USB_EP5Handler(USB_EventType Event) __attribute__((weak, alias("USB_EPxNullHandler")));
void USB_EP6Handler(USB_EventType Event) __attribute__((weak, alias("USB_EPxNullHandler")));
void USB_EP7Handler(USB_EventType Event) __attribute__((weak, alias("USB_EPxNullHandler")));

static const USB_EventHandlerDef USB_EventHandlers[USB_EP_COUNT] = {
    USB_EP0Handler,
    USB_EP1Handler,
    USB_EP2Handler,
    USB_EP3Handler,
    USB_EP4Handler,
    USB_EP5Handler,
    USB_EP6Handler,
    USB_EP7Handler,
};

static void USB_DispatchEvent(int EPIndex, USB_EventType Event)
{
    USB_EventHandlers[EPIndex](Event);
}

#if defined(STM32L1XX_MD) || defined(STM32L1XX_HD) || defined(STM32L1XX_MD_PLUS) || defined(STM32F37X)
void USB_LP_IRQHandler(void)
#else
void USB_LP_CAN1_RX0_IRQHandler(void)
#endif
{
    uint16_t Istr = USB->ISTR & (USB->CNTR | USB_ISTR_EP_ID | USB_ISTR_DIR);

    if (Istr & USB_ISTR_RESET) {
        USB->ISTR = (uint16_t)~USB_ISTR_RESET;
        USB_ResetHandler();
    }

    /* TODO: support other interrupts */

    /* Service the "correct transfer" int */
    while (Istr & USB_ISTR_CTR) {
        int EPIndex = Istr & USB_ISTR_EP_ID;
        uint16_t EPxR = USB_GetEPxR(EPIndex);
        /* Is the RX completion event set? */
        if (EPxR & USB_EPxR_CTR_RX) {
            if (EPxR & USB_EPxR_SETUP) {
                /* Handle a SETUP transaction completion */
                USB_DispatchEvent(EPIndex, USB_SETUP_EVENT);
            } else {
                /* Handle a OUT transaction completion */
                USB_DispatchEvent(EPIndex, USB_OUT_EVENT);
            }
        }
        /* Is the TX completion event set? */
        if (EPxR & USB_EPxR_CTR_TX) {
            /* Handle a IN transaction completion */
            USB_DispatchEvent(EPIndex, USB_IN_EVENT);
        }
        /* Clear the endpoint flags */
        USB_SetEPxR(EPIndex, EPxR & ~(USB_EPxR_CTR_RX | USB_EPxR_CTR_TX) & USB_EPxR_NONTOGGLED_MASK);
        /* Clear the int flag */
        USB->ISTR = (uint16_t)~USB_ISTR_CTR;
        /* Update the reg value; no masking needed */
        Istr = USB->ISTR;
    }
}
