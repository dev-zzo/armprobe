#include "usb_core.h"
#include "debug.h"

/******************************************************************************/
/* Control endpoint 0 handling code -- application specific                   */
/******************************************************************************/

void led_activity(int state);

#define ARRAY_SIZE(foo) (sizeof(foo)/sizeof(foo[0]))

const struct {
    USB_CONFIGURATION_DESCRIPTOR Config;
    USB_INTERFACE_DESCRIPTOR Interface0;
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
        0, /* NumEndpoints */
        USB_DEVICE_CLASS_VENDOR_SPECIFIC, /* InterfaceClass */
        0x00, /* InterfaceSubClass */
        0x00, /* InterfaceProtocol */
        5, /* iInterface */
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
static struct {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint16_t bString[24];
} USB_StringSerialDescriptor;
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
    (USB_STRING_DESCRIPTOR *)&USB_StringSerialDescriptor,
    &USB_StringConfig1Descriptor,
    &USB_StringIface0Descriptor,
};

const USB_DEVICE_DESCRIPTOR USB_DeviceDescriptor = {
    sizeof(USB_DEVICE_DESCRIPTOR),
    USB_DEVICE_DESCRIPTOR_TYPE,
    0x0110,
    USB_DEVICE_CLASS_VENDOR_SPECIFIC,
    0x00, /* bDeviceSubClass */
    0x00, /* bDeviceProtocol */
    USB_EP0_SIZE,
    0xDECA, /* idVendor */
    0x0002, /* idProduct */
    0x0100,
    1, /* iManufacturer */
    2, /* iProduct */
    3, /* iSerialNumber */
    1, /* bNumConfigurations */
};

static char USB_NibbleToChar(int nibble)
{
    return 'A' + nibble;
}

static void USB_UpdateSerial(void)
{
    const uint8_t *Serial = (const uint8_t *)0x1FFFF7E8;
    uint16_t *SerialStr = &USB_StringSerialDescriptor.bString[0];
    for (int i = 0; i < 12; ++i) {
        *SerialStr++ = USB_NibbleToChar((*Serial) >> 4);
        *SerialStr++ = USB_NibbleToChar((*Serial) & 0x0F);
        Serial++;
    }
    USB_StringSerialDescriptor.bLength = sizeof(USB_STRING_DESCRIPTOR) + 2 * 24;
    USB_StringSerialDescriptor.bDescriptorType = USB_STRING_DESCRIPTOR_TYPE;
}

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
        if (Index == 3) {
            USB_UpdateSerial();
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
        /* Nothing here for now */
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

void cmd_swd_enable(int enabled);
void cmd_switch_to_swd(void);
int cmd_swd_read(uint8_t cmd_request, void *DataBuffer);
int cmd_swd_write(uint8_t cmd_request, const void *DataBuffer);
void cmd_gpio_configure(int enabled);
void cmd_gpio_control(uint8_t bits);

#define APP_REQUEST_PING 0
#define APP_REQUEST_CONFIGURE_SWJ 1
#define APP_REQUEST_SWITCH_TO_SWD 2
#define APP_REQUEST_READ 3
#define APP_REQUEST_WRITE 4
#define APP_REQUEST_GPIO_CONFIGURE 5
#define APP_REQUEST_GPIO_CONTROL 6
#define APP_REQUEST_GET_STATUS 7

static uint8_t DataBuffer[4];
static int OpResult;

BOOL USB_EP0SetupVendorRequestHandler(void)
{
    switch (USB_SetupPacket.Request) {

    case APP_REQUEST_PING:
        USB_EP0ArmForStatusIn();
        return TRUE;

    case APP_REQUEST_CONFIGURE_SWJ:
        cmd_swd_enable(!!USB_SetupPacket.Value.Raw);
        USB_EP0ArmForStatusIn();
        return TRUE;

    case APP_REQUEST_SWITCH_TO_SWD:
        cmd_switch_to_swd();
        USB_EP0ArmForStatusIn();
        return TRUE;

    case APP_REQUEST_READ:
        OpResult = cmd_swd_read(USB_SetupPacket.Value.Raw & 0xFF, &DataBuffer[0]);
        if (OpResult) {
            return FALSE;
        }
        USB_EP0SetupDataIn(&DataBuffer[0], 4, USB_SetupPacket.Length);
        return TRUE;

    case APP_REQUEST_WRITE:
        USB_EP0SetupDataOut(&DataBuffer[0], 4, USB_SetupPacket.Length);
        return TRUE;

    case APP_REQUEST_GPIO_CONFIGURE:
        cmd_gpio_configure(!!USB_SetupPacket.Value.Raw);
        USB_EP0ArmForStatusIn();
        return TRUE;

    case APP_REQUEST_GPIO_CONTROL:
        cmd_gpio_control(USB_SetupPacket.Value.Raw & 0xFF);
        USB_EP0ArmForStatusIn();
        return TRUE;

    case APP_REQUEST_GET_STATUS:
        USB_EP0SetupDataIn(&OpResult, 1, USB_SetupPacket.Length);
        return TRUE;

    default:
        break;
    }
    return FALSE;
}

BOOL USB_EP0OutTransferCompletionHandler(void)
{
    if (USB_SetupPacket.RequestType.Type == USB_REQUEST_VENDOR) {
        switch (USB_SetupPacket.Request) {
        case APP_REQUEST_WRITE:
            OpResult = cmd_swd_write(USB_SetupPacket.Value.Raw & 0xFF, &DataBuffer[0]);
            if (OpResult) {
                led_activity(1);
                return FALSE;
            }
            return TRUE;

        default:
            break;
        }
    }
    return USB_EP0OutTransferCompletionStdHandler();
}
