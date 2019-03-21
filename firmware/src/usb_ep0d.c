#include "usb_core.h"
#include "debug.h"

/******************************************************************************/
/* Control endpoint 0 handling code -- not application specific               */
/******************************************************************************/

/* Preserved SETUP packet */
USB_SetupPacketDef USB_SetupPacket;
/* Transfer state variables */
void *USB_EP0TxRxData;
uint16_t USB_EP0TxRxCount;
BOOL USB_EP0LastPacket;
BOOL USB_EP0ZLPNeeded;
/* Current status of the device */
uint16_t USB_DeviceStatus = 0;
/* Current configuration set for the device */
uint8_t USB_DeviceConfiguration = 0;

void USB_EP0Stall(void)
{
    DEBUG_PrintString(" STALL");
    USB_SetEPRxTxStatus(0, USB_EPxR_STAT_RX_STALL | USB_EPxR_STAT_TX_STALL);
    USB_EP0TxRxCount = 0;
}

void USB_EP0ArmForSetup(void)
{
    /* Arm the endpoint */
    USB_SetEPRxTxStatus(0, USB_EPxR_STAT_RX_VALID | USB_EPxR_STAT_TX_NAK);
}

void USB_EP0DataOutStage(void)
{
    uint16_t Transferred;
    /* Transfer the data */
    Transferred = USB_EndpointToUserMemcpy(0, USB_EP_BUFFER_RX, USB_EP0TxRxData, USB_EP0TxRxCount);
    USB_EP0TxRxData += Transferred;
    USB_EP0TxRxCount -= Transferred;
    /* Arm the endpoint */
    USB_SetEPRxStatus(0, USB_EPxR_STAT_RX_VALID);
}

void USB_EP0DataInStage(void)
{
    uint16_t Transferred;
    /* NOTE: It is not possible to determine the size of the TX buffer automatically */
    Transferred = USB_EP0TxRxCount > USB_EP0_SIZE ? USB_EP0_SIZE : USB_EP0TxRxCount;
    /* Will this be the last transfer? */
    USB_EP0LastPacket = Transferred < USB_EP0_SIZE;
    /* Transfer the data */
    USB_UserToEndpointMemcpy(0, USB_EP_BUFFER_TX, USB_EP0TxRxData, Transferred);
    USB_EP0TxRxData += Transferred;
    USB_EP0TxRxCount -= Transferred;
    /* Arm the endpoint */
    USB_SetEPTxStatus(0, USB_EPxR_STAT_TX_VALID);
}

void USB_EP0SetupDataOut(void *Buffer, uint16_t AvailableCount, uint16_t RequestedCount)
{
    USB_EP0TxRxData = Buffer;
    USB_EP0TxRxCount = AvailableCount > RequestedCount ? RequestedCount : AvailableCount;
    USB_EP0LastPacket = FALSE;
    DEBUG_PrintString(" ArmO ");DEBUG_PrintU16(USB_EP0TxRxCount);
    /* Arm the endpoint */
    USB_SetEPRxStatus(0, USB_EPxR_STAT_RX_VALID);
}

void USB_EP0SetupDataIn(const void *Buffer, uint16_t AvailableCount, uint16_t RequestedCount)
{
    USB_EP0TxRxData = (void *)Buffer;
    USB_EP0TxRxCount = AvailableCount > RequestedCount ? RequestedCount : AvailableCount;
    USB_EP0LastPacket = FALSE;
    USB_EP0ZLPNeeded = RequestedCount != AvailableCount;
    DEBUG_PrintString(" ArmI ");DEBUG_PrintU16(USB_EP0TxRxCount);
    /* Transfer data and arm the endpoint */
    USB_EP0DataInStage();
    /* Also allow the host to terminate early */
    USB_SetEPRxStatus(0, USB_EPxR_STAT_RX_VALID);
}

BOOL USB_EP0OutTransferCompletionHandler(void) __attribute__((weak, alias("USB_EP0OutTransferCompletionStdHandler")));
BOOL USB_EP0OutTransferCompletionStdHandler(void)
{
    return TRUE;
}

void USB_EP0ArmForStatusOut(void)
{
    DEBUG_PrintString(" Arm SO");
    /* Arm the endpoint */
    USB_SetEPRxStatus(0, USB_EPxR_STAT_RX_VALID);
}

void USB_EP0ArmForStatusIn(void)
{
    DEBUG_PrintString(" Arm SI");
    /* Dummy copying to set the COUNT_TX value */
    USB_UserToEndpointMemcpy(0, USB_EP_BUFFER_TX, NULL, 0);
    /* Arm the endpoint */
    USB_SetEPTxStatus(0, USB_EPxR_STAT_TX_VALID);
}

void USB_EP0StatusCompletionHandler(void) __attribute__((weak, alias("USB_EP0StatusCompletionStdHandler")));
void USB_EP0StatusCompletionStdHandler(void)
{
    if (USB_SetupPacket.RequestType.Type == USB_REQUEST_STANDARD && USB_SetupPacket.Request == USB_REQUEST_SET_ADDRESS) {
        /* Update the device address */
        USB->DADDR = USB_DADDR_EF | USB_SetupPacket.Value.SetAddress.Address;
        DEBUG_PrintString(" ADDR SET: "); DEBUG_PrintU8(USB_SetupPacket.Value.SetAddress.Address);
    }
    /* Arm the endpoint */
    USB_EP0ArmForSetup();
    DEBUG_PrintString(" STA");
}

BOOL USB_SetConfiguration(unsigned Configuration);

BOOL USB_GetDescriptor(unsigned Type, unsigned Index, const void **pData, uint16_t *pLength);

static BOOL USB_EP0UnimplementedHandler(void)
{
    return FALSE;
}

BOOL USB_EP0SetupDeviceRequestHandler(void)
{
    const void *Data;
    uint16_t Length;

    switch (USB_SetupPacket.Request) {
        
    case USB_REQUEST_SET_ADDRESS:
        /* Handled on completion */
        USB_EP0ArmForStatusIn();
        return TRUE;

    case USB_REQUEST_GET_STATUS:
        Data = &USB_DeviceStatus;
        Length = 2;
        break;
        
    case USB_REQUEST_GET_CONFIGURATION:
        Data = &USB_DeviceConfiguration;
        Length = 1;
        break;
        
    case USB_REQUEST_SET_CONFIGURATION:
        if (!USB_SetConfiguration(USB_SetupPacket.Value.SetConfiguration.Index)) {
            return FALSE;
        }
        USB_EP0ArmForStatusIn();
        return TRUE;
    
    case USB_REQUEST_GET_DESCRIPTOR:
        if (!USB_GetDescriptor(USB_SetupPacket.Value.GetDescriptor.Type, USB_SetupPacket.Value.GetDescriptor.Index, &Data, &Length)) {
            return FALSE;
        }
        break;
        
    case USB_REQUEST_SET_DESCRIPTOR:
        /* Not implemented */
    case USB_REQUEST_CLEAR_FEATURE:
    case USB_REQUEST_SET_FEATURE:
        /* Not implemented */
    default:
        return FALSE;
    }

    USB_EP0SetupDataIn(Data, Length, USB_SetupPacket.Length);
    return TRUE;
}

BOOL USB_EP0SetupInterfaceRequestHandler(void) __attribute__((weak, alias("USB_EP0UnimplementedHandler")));

BOOL USB_EndpointHalt(unsigned EPIndex, unsigned Direction, BOOL State)
{
    if (Direction) {
        /* OUT endpoint */
        switch (USB_GetEPRxStatus(EPIndex)) {
        case USB_EPxR_STAT_RX_DIS:
            /* STALL if this refers to a disabled endpoint */
            return FALSE;
        case USB_EPxR_STAT_RX_STALL:
            /* Set the status to VALID */
            USB_ClearEPRxDTOG(EPIndex);
            USB_SetEPRxStatus(EPIndex, USB_EPxR_STAT_RX_VALID);
            break;
        default:
            /* No action */
            break;
        }
    } else {
        /* IN endpoint */
        switch (USB_GetEPTxStatus(EPIndex)) {
        case USB_EPxR_STAT_TX_DIS:
            /* STALL if this refers to a disabled endpoint */
            return FALSE;
        case USB_EPxR_STAT_TX_STALL:
            /* Set the status to VALID */
            USB_ClearEPTxDTOG(EPIndex);
            USB_SetEPTxStatus(EPIndex, USB_EPxR_STAT_TX_VALID);
            break;
        default:
            /* No action */
            break;
        }
    }
    return TRUE;
}

BOOL USB_EndpointSetClearFeature(unsigned Feature, BOOL State)
{
    switch (Feature) {
    case USB_FEATURE_ENDPOINT_HALT:
        return USB_EndpointHalt(USB_SetupPacket.Index.Endpoint.Number, !USB_SetupPacket.Index.Endpoint.Dir, State);
    default:
        return FALSE;
    }
}

BOOL USB_EP0SetupEndpointRequestHandler(void)
{
    switch (USB_SetupPacket.Request) {
        
    case USB_REQUEST_SET_FEATURE:
        return USB_EndpointSetClearFeature(USB_SetupPacket.Value.XxxFeature.Feature, TRUE);
    case USB_REQUEST_CLEAR_FEATURE:
        return USB_EndpointSetClearFeature(USB_SetupPacket.Value.XxxFeature.Feature, FALSE);
        
    case USB_REQUEST_GET_STATUS:
        /* TODO */
    case USB_REQUEST_SYNC_FRAME:
        /* TODO */
    default:
        return FALSE;
    }
}

BOOL USB_EP0SetupClassRequestHandler(void) __attribute__((weak, alias("USB_EP0UnimplementedHandler")));
BOOL USB_EP0SetupVendorRequestHandler(void) __attribute__((weak, alias("USB_EP0UnimplementedHandler")));

/*
 The SETUP handler should:
 - Save the SETUP packet
 - Set the endpoint to NAK all further requests
 - Dispatch the handling depending on the request
 The handler functions return a BOOL indicating whether handling was successful.
 If handling failed, the endpoint will be stalled.
 */
void USB_EP0SetupHandler(void)
{
    BOOL Result = FALSE;
    /* Save the packet */
    USB_EndpointToUserMemcpy(0, USB_EP_BUFFER_RX, &USB_SetupPacket, sizeof(USB_SetupPacket));
    DEBUG_PrintString("SETUP: "); DEBUG_PrintHex(&USB_SetupPacket, sizeof(USB_SetupPacket));
    /* Set the endpoint to NAK any further packets */
    USB_SetEPRxTxStatus(0, USB_EPxR_STAT_RX_NAK | USB_EPxR_STAT_TX_NAK);
    /* Dispatch the request */
    switch (USB_SetupPacket.RequestType.Type) {
    case USB_REQUEST_STANDARD:
        switch (USB_SetupPacket.RequestType.Recipient) {
        case USB_REQUEST_TO_DEVICE:
            Result = USB_EP0SetupDeviceRequestHandler();
            break;
        case USB_REQUEST_TO_INTERFACE:
            Result = USB_EP0SetupInterfaceRequestHandler();
            break;
        case USB_REQUEST_TO_ENDPOINT:
            Result = USB_EP0SetupEndpointRequestHandler();
            break;
        case USB_REQUEST_TO_OTHER:
            break;
        }
        break;
    case USB_REQUEST_CLASS:
        /* Handled entirely by the application */
        Result = USB_EP0SetupClassRequestHandler();
        break;
    case USB_REQUEST_VENDOR:
        /* Handled entirely by the application */
        Result = USB_EP0SetupVendorRequestHandler();
        break;
    default:
        break;
    }
    
    /* FALSE means something went wrong, STALL the endpoint. */
    if (!Result) {
        USB_EP0Stall();
    }
}

void USB_EP0OutHandler(void)
{
    DEBUG_PrintString("OUT");
    if (USB_SetupPacket.RequestType.Dir == USB_REQUEST_HOST_TO_DEVICE) {
        /* Host was writing; one of the data transfer stages has completed */
        /* Transfer the data into the buffer */
        USB_EP0DataOutStage();
        if (USB_EP0TxRxCount == 0) {
            /* Transfer complete */
            if (USB_EP0OutTransferCompletionHandler()) {
                /* Arm for the status stage */
                USB_EP0ArmForStatusIn();
            } else {
                /* Stall the status stage */
                USB_EP0Stall();
            }
        }
    } else {
        /* The status stage has completed */
        USB_EP0StatusCompletionHandler();
    }
}

void USB_EP0InHandler(void)
{
    DEBUG_PrintString("IN");
    if (USB_SetupPacket.RequestType.Dir == USB_REQUEST_DEVICE_TO_HOST) {
        DEBUG_PrintString(" to go: "); DEBUG_PrintU32(USB_EP0TxRxCount);
        /* Host was reading; one of the data transfer stages has completed */
        if (USB_EP0TxRxCount > 0) {
            /* Transfer the data, if any, into the buffer */
            USB_EP0DataInStage();
        } else {
            if (USB_EP0LastPacket) {
                USB_EP0LastPacket = FALSE;
                /* Arm for the status stage */
                USB_EP0ArmForStatusOut();
            } else {
                if (USB_EP0ZLPNeeded) {
                    USB_EP0ZLPNeeded = FALSE;
                    /* This is the zero-length packet stage */
                    USB_EP0DataInStage();
                } else {
                    /* Arm for the status stage */
                    USB_EP0ArmForStatusOut();
                }
            }
        }
    } else {
        /* The status stage has completed */
        USB_EP0StatusCompletionHandler();
    }
}

/*
 This is the main entry point for handling endpoint zero related completion events.
 The function dispatches the handling based on the event type.
 */
void USB_EP0Handler(USB_EventType Event)
{
    DEBUG_PrintString("EP0S ");

    switch (Event) {
    case USB_SETUP_EVENT:
        USB_EP0SetupHandler();
        break;
    case USB_OUT_EVENT:
        USB_EP0OutHandler();
        break;
    case USB_IN_EVENT:
        USB_EP0InHandler();
        break;
    }
    DEBUG_PrintString(" EP0E\r\n\r\n");
}
