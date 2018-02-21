#ifndef __stm32_usbcore_h
#define __stm32_usbcore_h

/******************************************************************************/
/* Hardware abstraction and support code                                      */
/******************************************************************************/

#include <stm32f10x.h>
#include <stdint.h>
#include <stddef.h>

#include "usb_defs.h"

typedef enum { FALSE, TRUE } BOOL;

/*******************  USB peripheral registers  *******************************/

typedef struct
{
    struct
    {
        __IO uint16_t R;
        uint16_t  RESERVED;
    } EPxR[8];
    uint32_t  RESERVED0[8];
    __IO uint16_t CNTR;
    uint16_t  RESERVED1;
    __IO uint16_t ISTR;
    uint16_t  RESERVED2;
    __IO uint16_t FNR;
    uint16_t  RESERVED3;
    __IO uint16_t DADDR;
    uint16_t  RESERVED4;
    __IO uint16_t BTABLE;
    uint16_t  RESERVED5;
} USB_TypeDef;

/* USB peripheral registers base address */
#define USB_BASE                            (APB1PERIPH_BASE + 0x5C00U)
#define USB                                 ((USB_TypeDef *) USB_BASE)

#define USB_EP_COUNT                        8

/*******************  Bit definition for USB_EPxR register  *******************/

#define USB_EPxR_EA                         ((uint16_t)0x000F)            /*!< Endpoint Address */

#define USB_EPxR_STAT_TX                    ((uint16_t)0x0030)            /*!< STAT_TX[1:0] bits (Status bits, for transmission transfers) */
#define USB_EPxR_STAT_TX_0                  ((uint16_t)0x0010)            /*!< Bit 0 */
#define USB_EPxR_STAT_TX_1                  ((uint16_t)0x0020)            /*!< Bit 1 */

#define USB_EPxR_DTOG_TX                    ((uint16_t)0x0040)            /*!< Data Toggle, for transmission transfers */
#define USB_EPxR_CTR_TX                     ((uint16_t)0x0080)            /*!< Correct Transfer for transmission */
#define USB_EPxR_EP_KIND                    ((uint16_t)0x0100)            /*!< Endpoint Kind */

#define USB_EPxR_EP_TYPE                    ((uint16_t)0x0600)            /*!< EP_TYPE[1:0] bits (Endpoint type) */
#define USB_EPxR_EP_TYPE_0                  ((uint16_t)0x0200)            /*!< Bit 0 */
#define USB_EPxR_EP_TYPE_1                  ((uint16_t)0x0400)            /*!< Bit 1 */

#define USB_EPxR_SETUP                      ((uint16_t)0x0800)            /*!< Setup transaction completed */

#define USB_EPxR_STAT_RX                    ((uint16_t)0x3000)            /*!< STAT_RX[1:0] bits (Status bits, for reception transfers) */
#define USB_EPxR_STAT_RX_0                  ((uint16_t)0x1000)            /*!< Bit 0 */
#define USB_EPxR_STAT_RX_1                  ((uint16_t)0x2000)            /*!< Bit 1 */

#define USB_EPxR_DTOG_RX                    ((uint16_t)0x4000)            /*!< Data Toggle, for reception transfers */
#define USB_EPxR_CTR_RX                     ((uint16_t)0x8000)            /*!< Correct Transfer for reception */

/*******************  Human-readable bit definitions **************************/

#define USB_EPxR_NONTOGGLED_MASK     (USB_EPxR_CTR_RX|USB_EPxR_SETUP|USB_EPxR_EP_TYPE|USB_EPxR_EP_KIND|USB_EPxR_CTR_TX|USB_EPxR_EA)

#define USB_EPxR_EP_BULK          (0)
#define USB_EPxR_EP_CONTROL       (USB_EPxR_EP_TYPE_0)
#define USB_EPxR_EP_ISOCHRONOUS   (USB_EPxR_EP_TYPE_1)
#define USB_EPxR_EP_INTERRUPT     (USB_EPxR_EP_TYPE_0|USB_EPxR_EP_TYPE_1)

#define USB_EPxR_STAT_TX_DIS      (0)
#define USB_EPxR_STAT_TX_STALL    (USB_EPxR_STAT_TX_0)
#define USB_EPxR_STAT_TX_NAK      (USB_EPxR_STAT_TX_1)
#define USB_EPxR_STAT_TX_VALID    (USB_EPxR_STAT_TX_0|USB_EPxR_STAT_TX_1)

#define USB_EPxR_STAT_RX_DIS      (0)
#define USB_EPxR_STAT_RX_STALL    (USB_EPxR_STAT_RX_0)
#define USB_EPxR_STAT_RX_NAK      (USB_EPxR_STAT_RX_1)
#define USB_EPxR_STAT_RX_VALID    (USB_EPxR_STAT_RX_0|USB_EPxR_STAT_RX_1)

/*******************  Register manipulation convenience macros ****************/

#define USB_GetEPxR(EpNum) (USB->EPxR[(EpNum)].R)
#define USB_GetEPxRNonToggled(EpNum) (USB->EPxR[(EpNum)].R & USB_EPxR_NONTOGGLED_MASK)
#define USB_SetEPxR(EpNum, Value) USB->EPxR[(EpNum)].R = (Value)

#define USB_SetEPTxStatus(EpNum, State) \
    { \
        uint16_t RegVal = USB_GetEPxR(EpNum) & (USB_EPxR_NONTOGGLED_MASK | USB_EPxR_STAT_TX); \
        USB_SetEPxR((EpNum), (RegVal ^ (State)) | USB_EPxR_CTR_RX | USB_EPxR_CTR_TX); \
    }

#define USB_SetEPRxStatus(EpNum, State) \
    { \
        uint16_t RegVal = USB_GetEPxR(EpNum) & (USB_EPxR_NONTOGGLED_MASK | USB_EPxR_STAT_RX); \
        USB_SetEPxR((EpNum), (RegVal ^ (State)) | USB_EPxR_CTR_RX | USB_EPxR_CTR_TX); \
    }

#define USB_SetEPRxTxStatus(EpNum, State) \
    { \
        uint16_t RegVal = USB_GetEPxR(EpNum) & (USB_EPxR_NONTOGGLED_MASK | USB_EPxR_STAT_RX | USB_EPxR_STAT_TX); \
        USB_SetEPxR((EpNum), (RegVal ^ (State)) | USB_EPxR_CTR_RX | USB_EPxR_CTR_TX); \
    }

#define USB_GetEPRxStatus(EpNum) \
    ((uint16_t)USB_GetEPxR(EpNum) & USB_EPxR_STAT_RX)
#define USB_GetEPTxStatus(EpNum) \
    ((uint16_t)USB_GetEPxR(EpNum) & USB_EPxR_STAT_TX)

#define USB_ToggleEPRxDTOG(EpNum) \
    USB_SetEPxR(EpNum, USB_GetEPxRNonToggled(EpNum) | USB_EPxR_CTR_RX | USB_EPxR_CTR_TX | USB_EPxR_DTOG_RX)
#define USB_ToggleEPTxDTOG(EpNum) \
    USB_SetEPxR(EpNum, USB_GetEPxRNonToggled(EpNum) | USB_EPxR_CTR_RX | USB_EPxR_CTR_TX | USB_EPxR_DTOG_TX)

#define USB_ClearEPRxDTOG(EpNum) \
    { \
        uint16_t RegVal = USB_GetEPxR(EpNum) & (USB_EPxR_NONTOGGLED_MASK | USB_EPxR_DTOG_RX); \
        USB_SetEPxR((EpNum), RegVal | USB_EPxR_CTR_RX | USB_EPxR_CTR_TX); \
    }
#define USB_ClearEPTxDTOG(EpNum) \
    { \
        uint16_t RegVal = USB_GetEPxR(EpNum) & (USB_EPxR_NONTOGGLED_MASK | USB_EPxR_DTOG_TX); \
        USB_SetEPxR((EpNum), RegVal | USB_EPxR_CTR_RX | USB_EPxR_CTR_TX); \
    }

#define USB_SetEPAddress(EpNum, Address) \
    USB_SetEPxR((EpNum), (USB_GetEPxRNonToggled(EpNum) & (~USB_EPxR_EA)) | USB_EPxR_CTR_RX | USB_EPxR_CTR_TX | (Address))

#define USB_DisableEP(EpNum) \
    USB_SetEPxR(EpNum, USB_GetEPxR(EpNum) & ~USB_EPxR_NONTOGGLED_MASK)

/*******************  USB packet memory area (PMA)  ****************************/

typedef struct {
    __IO uint16_t ADDR_TX;
    uint16_t RESERVED0;
    __IO uint16_t COUNT_TX;
    uint16_t RESERVED1;
} USB_TxDescriptor;

typedef struct {
    __IO uint16_t ADDR_RX;
    uint16_t RESERVED0;
    __IO uint16_t COUNT_RX : 10;
    __IO uint16_t NUM_BLOCK : 5;
    __IO uint16_t BLOCK_SIZE : 1;
    uint16_t RESERVED1;
} USB_RxDescriptor;

/* USB Packet Memory Area base address */
#define PMA_BASE    (APB1PERIPH_BASE + 0x6000U)
#define USB_BTABLE   0

#define USB_EP_BUFFER_TX 0
#define USB_EP_BUFFER_RX 1

#define USB_GetTxDescriptor(EPIndex, BufIndex) \
    (&((USB_TxDescriptor *)PMA_BASE)[(EPIndex) * 2 + (BufferIndex)])
#define USB_GetRxDescriptor(EPIndex, BufIndex) \
    (&((USB_RxDescriptor *)PMA_BASE)[(EPIndex) * 2 + (BufferIndex)])

/*********************** External USB core API  *******************************/

typedef enum {
    USB_SETUP_EVENT,
    USB_OUT_EVENT,
    USB_IN_EVENT,
} USB_EventType;

void USB_Init();
void USB_ConfigureTxBuffer(unsigned EPIndex, unsigned BufferIndex, uint16_t Address);
void USB_ConfigureRxBuffer(unsigned EPIndex, unsigned BufferIndex, uint16_t Address, uint16_t Size);
void USB_UserToEndpointMemcpy(unsigned EPIndex, unsigned BufferIndex, const void *UserBuffer, uint16_t DataSize);
uint16_t USB_EndpointToUserMemcpy(unsigned EPIndex, unsigned BufferIndex, void *UserBuffer, uint16_t BufferSize);

void USB_EP0Handler(USB_EventType Event);

/*********************** Configuration parameters *****************************/

/* Bitmask of interrupts handled during normal ops; from USB_CNTR_* */
#define USB_HANDLED_INTS (0)

#define USB_EP0_SIZE 8
#define USB_EP1_SIZE 12
#define USB_EP2_SIZE 12

#if 1
#define DEBUG_PrintString(s) DEBUG_PrintString_(s)
#define DEBUG_PrintHex(p,n) DEBUG_PrintHex_((const void *)(p), (n))
void DEBUG_PrintString_(const char *s);
void DEBUG_PrintHex_(const void *p, unsigned n);
#else
#define DEBUG_PrintString(s)
#define DEBUG_PrintHex(p,n)
#endif

#endif /* __stm32_usbcore_h */
