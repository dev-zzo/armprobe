#include "usb_core.h"

/******************************************************************************/
/* Control endpoint 0 handling code -- application specific                   */
/******************************************************************************/

extern uint8_t USB_DeviceConfiguration;
void USB_EP0ArmForSetup(void);

#define ARRAY_SIZE(foo) (sizeof(foo)/sizeof(foo[0]))

const struct {
    USB_CONFIGURATION_DESCRIPTOR Config;
    USB_INTERFACE_DESCRIPTOR Interface0;
    USB_ENDPOINT_DESCRIPTOR EP1Out;
    USB_ENDPOINT_DESCRIPTOR EP2In;
} __attribute__((packed)) USB_Config1Descriptor = {
    {
        sizeof(USB_CONFIGURATION_DESCRIPTOR), /* Length */
        USB_CONFIGURATION_DESCRIPTOR_TYPE, /* DescriptorType */
        sizeof(USB_Config1Descriptor), /* TotalLength */
        1, /* NumInterfaces */
        1, /* ConfigurationValue */
        4, /* iConfiguration */
        USB_CONFIG_BUS_POWERED, /* Attributes */
        USB_CONFIG_POWER_MA(100), /* MaxPower */
    },
    {
        sizeof(USB_INTERFACE_DESCRIPTOR), /* Length */
        USB_INTERFACE_DESCRIPTOR_TYPE, /* DescriptorType */
        0, /* InterfaceNumber */
        0, /* AlternateSetting */
        2, /* NumEndpoints */
        USB_DEVICE_CLASS_VENDOR_SPECIFIC, /* InterfaceClass */
        0x00, /* InterfaceSubClass */
        0x00, /* InterfaceProtocol */
        5, /* iInterface */
    },
    {
        sizeof(USB_ENDPOINT_DESCRIPTOR), /* Length */
        USB_ENDPOINT_DESCRIPTOR_TYPE, /* DescriptorType */
        USB_ENDPOINT_OUT(1), /* EndpointAddress */
        USB_ENDPOINT_TYPE_BULK, /* Attributes */
        USB_EP1_SIZE, /* MaxPacketSize */
        0, /* Interval */
    },
    {
        sizeof(USB_ENDPOINT_DESCRIPTOR), /* Length */
        USB_ENDPOINT_DESCRIPTOR_TYPE, /* DescriptorType */
        USB_ENDPOINT_IN(2), /* EndpointAddress */
        USB_ENDPOINT_TYPE_BULK, /* Attributes */
        USB_EP2_SIZE, /* MaxPacketSize */
        0, /* Interval */
    },
};

const USB_CONFIGURATION_DESCRIPTOR * const USB_ConfigDescriptors[] = {
    &USB_Config1Descriptor.Config,
};

const USB_STRING_DESCRIPTOR USB_String0Descriptor = {
    sizeof(USB_STRING_DESCRIPTOR) + 2 * 1,
    USB_STRING_DESCRIPTOR_TYPE,
    { 0x0409, },
};
const USB_STRING_DESCRIPTOR USB_StringMfgDescriptor = {
    sizeof(USB_STRING_DESCRIPTOR) + 2 * 7,
    USB_STRING_DESCRIPTOR_TYPE,
    { 'd', 'e', 'v', '_', 'z', 'z', 'o', },
};
const USB_STRING_DESCRIPTOR USB_StringProdDescriptor = {
    sizeof(USB_STRING_DESCRIPTOR) + 2 * 15,
    USB_STRING_DESCRIPTOR_TYPE,
    { 'A', 'R', 'M', ' ', 'A', 'D', 'I', 'v', '5', ' ', 'P', 'r', 'o', 'b', 'e', },
};
const USB_STRING_DESCRIPTOR USB_StringSerialDescriptor = {
    sizeof(USB_STRING_DESCRIPTOR) + 2 * 1,
    USB_STRING_DESCRIPTOR_TYPE,
    { '1', },
};
const USB_STRING_DESCRIPTOR USB_StringConfig1Descriptor = {
    sizeof(USB_STRING_DESCRIPTOR) + 2 * 8,
    USB_STRING_DESCRIPTOR_TYPE,
    { 'C', 'o', 'n', 'f', 'i', 'g', ' ', '1', },
};
const USB_STRING_DESCRIPTOR USB_StringIface0Descriptor = {
    sizeof(USB_STRING_DESCRIPTOR) + 2 * 11,
    USB_STRING_DESCRIPTOR_TYPE,
    { 'I', 'n', 't', 'e', 'r', 'f', 'a', 'c', 'e', ' ', '0', },
};
const USB_STRING_DESCRIPTOR * const USB_StringDescriptors[] = {
    &USB_String0Descriptor,
    &USB_StringMfgDescriptor,
    &USB_StringProdDescriptor,
    &USB_StringSerialDescriptor,
    &USB_StringConfig1Descriptor,
    &USB_StringIface0Descriptor,
};

const USB_DEVICE_DESCRIPTOR USB_DeviceDescriptor = {
    sizeof(USB_DEVICE_DESCRIPTOR),
    USB_DEVICE_DESCRIPTOR_TYPE,
    0x0110,
    USB_DEVICE_CLASS_VENDOR_SPECIFIC,
    0x00,
    0x00,
    USB_EP0_SIZE,
    0xDECA, /* idVendor */
    0x0002, /* idProduct */
    0x0100,
    1,
    2,
    3,
    ARRAY_SIZE(USB_ConfigDescriptors),
};

BOOL USB_GetDescriptor(unsigned Type, unsigned Index, const void **pData, uint16_t *pLength)
{
    const void *Data;

    switch (Type) {
    case USB_DEVICE_DESCRIPTOR_TYPE:
        Data = &USB_DeviceDescriptor;
        *pLength = USB_DeviceDescriptor.bLength;
        break;

    case USB_CONFIGURATION_DESCRIPTOR_TYPE:
        if (Index >= ARRAY_SIZE(USB_ConfigDescriptors)) {
            return FALSE;
        }
        Data = USB_ConfigDescriptors[Index];
        *pLength = ((const USB_CONFIGURATION_DESCRIPTOR *)Data)->wTotalLength;
        break;
    
    case USB_STRING_DESCRIPTOR_TYPE:
        if (Index >= ARRAY_SIZE(USB_StringDescriptors)) {
            return FALSE;
        }
        Data = USB_StringDescriptors[Index];
        *pLength = ((const USB_STRING_DESCRIPTOR *)Data)->bLength;
        break;
        
    default:
        return FALSE;
    }
    *pData = Data;
    return TRUE;
}

void USB_Deconfigure(void)
{
    unsigned EPIndex;
    /* Disable all endpoints except the default control */
    for (EPIndex = 1; EPIndex < USB_EP_COUNT; ++EPIndex) {
        USB_DisableEP(EPIndex);
    }
}

#define USB_EP_BUFFERS_START (0x40)

#define USB_EP0_TX_BUFFER_AT (USB_EP_BUFFERS_START)
#define USB_EP0_RX_BUFFER_AT (USB_EP0_TX_BUFFER_AT + USB_EP0_SIZE)
#define USB_EP1_RX_BUFFER_AT (USB_EP0_RX_BUFFER_AT + USB_EP0_SIZE)
#define USB_EP2_TX_BUFFER_AT (USB_EP1_RX_BUFFER_AT + USB_EP1_SIZE)

BOOL USB_SetConfiguration(unsigned Configuration)
{
    /* Endpoint 0 stays configured */
    switch (Configuration) {
    case 0:
        /* The device leaves the CONFIGURED state */
        USB_Deconfigure();
        break;
    case 1:
        USB_Deconfigure();
        /* Configure the endpoints and buffer descriptors */
        USB_ConfigureRxBuffer(1, USB_EP_BUFFER_RX, USB_EP1_RX_BUFFER_AT, USB_EP1_SIZE);
        USB_ConfigureTxBuffer(2, USB_EP_BUFFER_TX, USB_EP2_TX_BUFFER_AT);
        USB_SetEPxR(1, USB_EPxR_EP_BULK | 1);
        USB_SetEPRxTxStatus(1, USB_EPxR_STAT_RX_VALID | USB_EPxR_STAT_TX_DIS);
        USB_SetEPxR(2, USB_EPxR_EP_BULK | 2);
        USB_SetEPRxTxStatus(2, USB_EPxR_STAT_RX_DIS | USB_EPxR_STAT_TX_NAK);
        break;
    default:
        /* Failed */
        return FALSE;
    }
    USB_DeviceConfiguration = Configuration;
    return TRUE;
}

void USB_EP0Configure(void)
{
    /* Configure buffers for endpoint 0 */
    USB_ConfigureTxBuffer(0, USB_EP_BUFFER_TX, USB_EP0_TX_BUFFER_AT);
    USB_ConfigureRxBuffer(0, USB_EP_BUFFER_RX, USB_EP0_RX_BUFFER_AT, USB_EP0_SIZE);
    /* Configure endpoint 0 */
    USB_SetEPxR(0, USB_EPxR_EP_CONTROL);
    USB_EP0ArmForSetup();
}
