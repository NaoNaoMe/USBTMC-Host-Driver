/*
 * USBTMC class driver for USB Host Shield 2.0 Library
 * Copyright (c) 2019 Naoya Imai
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */
#include "usbtmc.h"

const uint8_t USBTMC::epDataInIndex = 1;
const uint8_t USBTMC::epDataOutIndex = 2;
const uint8_t USBTMC::epInterruptInIndex = 3;

USBTMC::USBTMC(USB* p, USBTMCAsyncOper * pasync) : pAsync(pasync), pUsb(p), bAddress(0), bNumEP(1), bTag(1), CommandState(USBTMC_Idle)
{
    for (uint8_t i = 0; i < USBTMC_MAX_ENDPOINTS; i++)
    {
        epInfo[i].epAddr = 0;
        epInfo[i].maxPktSize = (i) ? 0 : 8;
        epInfo[i].bmSndToggle = 0;
        epInfo[i].bmRcvToggle = 0;
        epInfo[i].bmNakPower = (i == epDataInIndex) ? USB_NAK_NOWAIT : USB_NAK_MAX_POWER;
    }
    if (pUsb)
        pUsb->RegisterDeviceClass(this);
}

uint8_t USBTMC::Init(uint8_t parent, uint8_t port, bool lowspeed)
{
    const uint8_t constBufSize = sizeof(USB_DEVICE_DESCRIPTOR);

    uint8_t buf[constBufSize];
    USB_DEVICE_DESCRIPTOR* udd = reinterpret_cast<USB_DEVICE_DESCRIPTOR*>(buf);
    uint8_t rcode;
    UsbDevice* p = NULL;
    EpInfo* oldep_ptr = NULL;

    uint8_t num_of_conf; // number of configurations

    AddressPool & addrPool = pUsb->GetAddressPool();

    if (bAddress)
        return USB_ERROR_CLASS_INSTANCE_ALREADY_IN_USE;

    // Get pointer to pseudo device with address 0 assigned
    p = addrPool.GetUsbDevicePtr(0);

    if (!p)
        return USB_ERROR_ADDRESS_NOT_FOUND_IN_POOL;

    if (!p->epinfo)
        return USB_ERROR_EPINFO_IS_NULL;

    // Save old pointer to EP_RECORD of address 0
    oldep_ptr = p->epinfo;

    // Temporary assign new pointer to epInfo to p->epinfo in order to avoid toggle inconsistence
    p->epinfo = epInfo;

    p->lowspeed = lowspeed;

    // Get device descriptor
    rcode = pUsb->getDevDescr(0, 0, sizeof(USB_DEVICE_DESCRIPTOR), buf);

    // Restore p->epinfo
    p->epinfo = oldep_ptr;

    if (rcode)
        goto FailGetDevDescr;

    // Allocate new address according to device class
    bAddress = addrPool.AllocAddress(parent, false, port);

    if (!bAddress)
        return USB_ERROR_OUT_OF_ADDRESS_SPACE_IN_POOL;

    // Extract Max Packet Size from the device descriptor
    epInfo[0].maxPktSize = udd->bMaxPacketSize0;

    // Assign new address to the device
    rcode = pUsb->setAddr(0, 0, bAddress);

    if (rcode)
    {
        p->lowspeed = false;
        addrPool.FreeAddress(bAddress);
        bAddress = 0;
        return rcode;
    }

    p->lowspeed = false;

    p = addrPool.GetUsbDevicePtr(bAddress);

    if (!p)
        return USB_ERROR_ADDRESS_NOT_FOUND_IN_POOL;

    p->lowspeed = lowspeed;

    num_of_conf = udd->bNumConfigurations;

    // Assign epInfo to epinfo pointer
    rcode = pUsb->setEpInfoEntry(bAddress, 1, epInfo);

    if (rcode)
        goto FailSetDevTblEntry;

    for (uint8_t i = 0; i < num_of_conf; i++)
    {
        //USB Test and Measurement Device conforming to the USBTMC USB488 Subclass Specification found on www.usb.org.
        ConfigDescParser < USB_CLASS_APP_SPECIFIC, 0x03, 0x01, CP_MASK_COMPARE_ALL > confDescrParser(this);

        rcode = pUsb->getConfDescr(bAddress, 0, i, &confDescrParser);

        if (rcode)
            goto FailGetConfDescr;

        if (bNumEP > 1)
            break;
    } // for

    if (bNumEP < 2)
        return USB_DEV_CONFIG_ERROR_DEVICE_NOT_SUPPORTED;

    // Assign epInfo to epinfo pointer
    rcode = pUsb->setEpInfoEntry(bAddress, bNumEP, epInfo);

    // Set Configuration Value
    rcode = pUsb->setConf(bAddress, 0, bConfNum);

    if (rcode)
        goto FailSetConfDescr;

    // USBTMC Get Capabilities
    // bRequest = 0x07(7) GET_CAPABILITIES
    // wValLo = 0x00
    // wValHi = 0x00
    // total, nbytes = 0x0018
    rcode = pUsb->ctrlReq(bAddress, 0, bmREQ_CL_GET_INTF, 0x07, 0x00, 0x00, 0x0000, 0x0018, 0x0018, (uint8_t*)&Capabilities, NULL);

    if (rcode)
        goto FailOnInit;

    // Does the interface accept REN_CONTROL request?
    if(Capabilities.USB488Interface & 0x02)
    {
        // USBTMC REN_CONTROL
        // bRequest = 0xA0(160) REN_CONTROL
        // wValLo = 0x01 Assert REN.
        // wValHi = 0x00 Reserved. Must be 0x00.
        // total, nbytes = 0x0001
        uint8_t usbtmc_status;
        rcode = pUsb->ctrlReq(bAddress, 0, bmREQ_CL_GET_INTF, 0xA0, 0x01, 0x00, 0x0000, 0x0001, 0x0001, &usbtmc_status, NULL);

        if (rcode)
            goto FailOnInit;

        if (usbtmc_status != 0x01)
            goto FailOnInit;
    }

    return 0;

FailGetDevDescr:
# ifdef DEBUG_USB_HOST
    NotifyFailGetDevDescr();
    goto Fail;
#endif

FailSetDevTblEntry:
# ifdef DEBUG_USB_HOST
    NotifyFailSetDevTblEntry();
    goto Fail;
#endif

FailGetConfDescr:
# ifdef DEBUG_USB_HOST
    NotifyFailGetConfDescr();
    goto Fail;
#endif

FailSetConfDescr:
# ifdef DEBUG_USB_HOST
    NotifyFailSetConfDescr();
    goto Fail;
#endif

FailOnInit:
#ifdef DEBUG_USB_HOST

Fail:
    NotifyFail(rcode);
#endif
    Release();
    return rcode;
}

uint8_t USBTMC::Send(uint8_t nbytes, uint8_t* dataptr)
{
    uint8_t rcode = 0;
    uint8_t* tmpptr = dataptr;

    CommandState = USBTMC_Idle;
    for (uint8_t i = 0; i < nbytes; i++)
    {
        if (*tmpptr++ == '?')
        {
            CommandState = USBTMC_Request;
        }
    }

    rcode = BulkOut_Data(nbytes, dataptr);

    if (rcode)
    {
        pAsync->OnError(F("USBTMC Error on Send: rcode="), false);
        pAsync->OnError(String(rcode, HEX) + "h", true);
        CommandState = USBTMC_Idle;
    }

    return rcode;
}

void USBTMC::Run()
{
    uint8_t rcode = 0;
    uint8_t status = 0;;
    bool isFull = false;
    uint8_t bmAbortBulkIn = 0;

    switch (CommandState)
    {
        case USBTMC_Request:
            rcode = BulkOut_Request(USBTMC_REQUEST_SIZE);

            if (rcode)
            {
                CommandState = USBTMC_Idle;
            }
            else
            {
                WaitBeginMillis = millis();
                CommandState = USBTMC_Receive;
            }
            break;

        case USBTMC_Receive:
            uint8_t buf[USBTMC_REQUEST_SIZE];
            uint16_t rcvd;

            for (int i = 0; i < USBTMC_REQUEST_SIZE; i++)
                buf[i] = 0;
            rcvd = USBTMC_REQUEST_SIZE;

            rcode = BulkIn(&rcvd, buf);

            if (rcode == hrNAK)
            {
                //Try again
                unsigned long currentMillis;
                currentMillis = millis();
                if ((currentMillis - WaitBeginMillis) >= 5000)
                {
                    pAsync->OnError(F("USBTMC Timeout on Receive"), true);
                    CommandState = USBTMC_InitiateAbortBulkIn;
                }
                
            }
            else if (rcode)
            {
                CommandState = USBTMC_Idle;
            }
            else
            {
                CommandState = USBTMC_Request;
                for (uint16_t i = 0; i < rcvd; i++)
                {
                    if (pAsync->OnReceived(buf[i]))
                    {
                        CommandState = USBTMC_Idle;
                        break;
                    }
                }
            }

            break;

        case USBTMC_InitiateAbortBulkIn:
            rcode = InitiateAbortBulkIn(&status);

            if (rcode)
            {
                CommandState = USBTMC_Idle;
            }
            else
            {
                if(status == 0x01)  // STATUS_SUCCESS
                    CommandState = USBTMC_PurgingOnAbortBulkIn;
                else if(status == 0x81)  // STATUS_TRANSFER_NOT_IN_PROGRESS
                    CommandState = USBTMC_InitiateAbortBulkIn;
                else
                {
                    pAsync->OnError(F("USBTMC Error on USBTMC_InitiateAbortBulkIn: status="), false);
                    pAsync->OnError(String(status, HEX) + "h", true);
                    CommandState = USBTMC_Idle;
                }
            }

            break;

        case USBTMC_PurgingOnAbortBulkIn:
            rcode = PurgeBulkIn(isFull);

            if (rcode)
            {
                CommandState = USBTMC_Idle;
            }
            else
            {
                if(isFull)
                    CommandState = USBTMC_PurgingOnAbortBulkIn;
                else
                    CommandState = USBTMC_CheckAbortBulkInStatus;
            }

            break;

        case USBTMC_CheckAbortBulkInStatus:
            rcode = CheckAbortBulkInStatus(&status, &bmAbortBulkIn);

            if (rcode)
            {
                CommandState = USBTMC_Idle;
            }
            else
            {
                if(status != 0x02)
                    CommandState = USBTMC_Idle;
                else
                {
                    // USBTMC_STATUS_PENDING
                    if(bmAbortBulkIn & 0x01 == 0x01)
                        CommandState = USBTMC_PurgingOnAbortBulkIn;
                    else
                        CommandState = USBTMC_CheckAbortBulkInStatus;

                }
            }

            break;

        default:
            break;
    }
}

bool USBTMC::IsIdle()
{
    if (CommandState == USBTMC_Idle)
    {
        return true;
    }
    else
    {
        return false;
    }
}

void USBTMC::EndpointXtract(uint8_t conf, uint8_t iface, uint8_t alt, uint8_t proto __attribute__((unused)), const USB_ENDPOINT_DESCRIPTOR* pep) {
    ErrorMessage<uint8_t>(PSTR("Conf.Val"), conf);
    ErrorMessage<uint8_t>(PSTR("Iface Num"), iface);
    ErrorMessage<uint8_t>(PSTR("Alt.Set"), alt);

    bConfNum = conf;

    uint8_t index;

    if ((pep->bmAttributes & bmUSB_TRANSFER_TYPE) == USB_TRANSFER_TYPE_INTERRUPT && (pep->bEndpointAddress & 0x80) == 0x80)
            index = epInterruptInIndex;
    else if ((pep->bmAttributes & bmUSB_TRANSFER_TYPE) == USB_TRANSFER_TYPE_BULK)
            index = ((pep->bEndpointAddress & 0x80) == 0x80) ? epDataInIndex : epDataOutIndex;
    else
            return;

    // Fill in the endpoint info structure
    epInfo[index].epAddr = (pep->bEndpointAddress & 0x0F);
    epInfo[index].maxPktSize = (uint8_t)pep->wMaxPacketSize;
    epInfo[index].bmSndToggle = 0;
    epInfo[index].bmRcvToggle = 0;

    bNumEP++;

    PrintEndpointDescriptor(pep);
}

uint8_t USBTMC::Release()
{
    uint8_t rcode = 0;

    pUsb->GetAddressPool().FreeAddress(bAddress);

    bAddress = 0;
    bNumEP = 1;
    return rcode;
}

uint8_t USBTMC::BulkOut_Data(uint8_t nbytes, uint8_t* dataptr)
{
#define RESERVED_SIZE 12
#define DEV_MESSAGE_BEGIN RESERVED_SIZE
    uint8_t message[USBTMC_COMMAND_SIZE + RESERVED_SIZE];
    uint16_t messageSize = RESERVED_SIZE;
    uint8_t rcode = 0;

    if (nbytes > USBTMC_COMMAND_SIZE)
    {
        pAsync->OnError(F("USBTMC Error on BulkOut_Data: Message size must be less than USBTMC_COMMAND_SIZE"), true);
        rcode = hrUNDEF;
        return rcode;
    }

    messageSize += nbytes;

    //0:MsgID
    message[0] = 0x01; //DEV_DEP_MSG_OUT
    //1:bTag
    message[1] = bTag;
    //2:bTagInverse
    message[2] = ~bTag;
    //3:Reserved(0x00)
    message[3] = 0x00;
    //4,5,6,7:TransferSize
    message[4] = nbytes;
    message[5] = 0x00;
    message[6] = 0x00;
    message[7] = 0x00;
    //8:bmTransfer Attributes
    message[8] = 0x01; //(EOM is set)
    //9,10,11:Reserved(0x00)
    message[9] = 0x00;
    message[10] = 0x00;
    message[11] = 0x00;

    //Device dependent message data bytes
    for (uint16_t i = DEV_MESSAGE_BEGIN; i < messageSize; i++)
        message[i] = *dataptr++;

    uint16_t quotient = messageSize / 4;

    if (messageSize > (quotient * 4))
        quotient++;

    rcode = pUsb->outTransfer(bAddress, epInfo[epDataOutIndex].epAddr, (quotient * 4), &message[0]);
    if (rcode)
    {
        pAsync->OnError(F("USBTMC Error on BulkOut_Data: rcode="), false);
        pAsync->OnError(String(rcode, HEX) + "h", true);
        return rcode;
    }

    last_bTag = bTag;
    bTag++;
    if(bTag == 0)
    {
        //The Host must set bTag such that 1<=bTag<=255.
        bTag = 1;
    }
    
    return rcode;

#undef DEV_MESSAGE_BEGIN
#undef RESERVED_SIZE
}

uint8_t USBTMC::BulkOut_Request(uint8_t nbytes)
{
#define MESSAGE_SIZE 12
    uint8_t message[MESSAGE_SIZE];
    uint16_t messageSize = MESSAGE_SIZE;
    uint8_t rcode = 0;

    //0:MsgID
    message[0] = 0x02; //REQUEST_DEV_DEP_MSG_IN
    //1:bTag
    message[1] = bTag;
    //2:bTagInverse
    message[2] = ~bTag;
    //3:Reserved(0x00)
    message[3] = 0x00;
    //4,5,6,7:TransferSize
    message[4] = nbytes;
    message[5] = 0x00;
    message[6] = 0x00;
    message[7] = 0x00;
    //8:bmTransfer Attributes
    message[8] = 0x00; //D1 = 0 The device must ignore TermChar.
    //9:TermChar
    message[9] = 0x00; //If bmTransferAttributes.D1 = 0, the device must ignore this field.
    //10,11:Reserved(0x00)
    message[10] = 0x00;
    message[11] = 0x00;

    rcode = pUsb->outTransfer(bAddress, epInfo[epDataOutIndex].epAddr, messageSize, &message[0]);
    if (rcode)
    {
        pAsync->OnError(F("USBTMC Error on BulkOut_Request: rcode="), false);
        pAsync->OnError(String(rcode, HEX) + "h", true);
        return rcode;
    }
    
    last_bTag = bTag;
    bTag++;
    if(bTag == 0)
    {
        //The Host must set bTag such that 1<=bTag<=255.
        bTag = 1;
    }
    
    return rcode;

#undef MESSAGE_SIZE
}

uint8_t USBTMC::BulkIn(uint16_t* bytes_rcvd, uint8_t* dataptr)
{
#define EXPECTED_SIZE 12

    uint8_t packet_size = epInfo[epDataInIndex].maxPktSize;
    uint8_t message[packet_size];
    uint16_t rcvd = packet_size;
    uint8_t rcode = 0;

    rcode = pUsb->inTransfer(bAddress, epInfo[epDataInIndex].epAddr, &rcvd, message);
    if (rcode == hrNAK)
    {
        *bytes_rcvd = 0;
        return rcode;      
    }
    if (rcode)
    {
        pAsync->OnError(F("USBTMC Error on BulkIn: rcode="), false);
        pAsync->OnError(String(rcode, HEX) + "h", true);
        *bytes_rcvd = 0;
        return rcode;
    }

    if (rcvd < EXPECTED_SIZE)
    {
        pAsync->OnError(F("USBTMC Error on BulkIn: Received unexpected size"), true);
        *bytes_rcvd = 0;
        rcode = hrUNDEF;
        return rcode;
    }

    uint32_t data_size;

    //4,5,6,7:TransferSize
    data_size = message[7];
    data_size = data_size << 8;
    data_size += message[6];
    data_size = data_size << 8;
    data_size += message[5];
    data_size = data_size << 8;
    data_size += message[4];

    if (data_size > *bytes_rcvd)
    {
        pAsync->OnError(F("USBTMC Error on BulkIn: Received transferSize is overflow in packet"), true);
        *bytes_rcvd = 0;
        rcode = hrUNDEF;
        return rcode;
    }

    for (uint32_t i = 0; i < data_size; i++)
        *dataptr++ = message[i + EXPECTED_SIZE];

    *bytes_rcvd = (uint8_t)data_size;

    return rcode;

#undef EXPECTED_SIZE
}

uint8_t USBTMC::PurgeBulkIn(bool &isFull)
{
    uint8_t packet_size = epInfo[epDataInIndex].maxPktSize;
    uint8_t message[packet_size];
    uint16_t rcvd = packet_size;
    uint8_t rcode = 0;

    rcode = pUsb->inTransfer(bAddress, epInfo[epDataInIndex].epAddr, &rcvd, message);
    if (rcode)
    {
        pAsync->OnError(F("USBTMC Error on PurgeBulkIn: rcode=" ), false);
        pAsync->OnError(String(rcode, HEX) + "h", true);
        return rcode;
    }

    if (rcvd >= packet_size)
    {
        isFull = true;
    }
    else
    {
        isFull = false;
    }

    return rcode;

}

uint8_t USBTMC::InitiateAbortBulkIn(uint8_t* status)
{
    uint8_t rcode = 0;

    // USBTMC INITIATE ABORT BULKIN
    // bRequest = 0x03(3) Initiate Abort BulkIn
    // wValLo = bTag.
    // wValHi = 0x00 Reserved. Must be 0x00.
    // total, nbytes = 0x0002
    uint8_t response[2];
    uint16_t wInd = 0x80 + epInfo[epDataInIndex].epAddr;
    rcode = pUsb->ctrlReq(bAddress, 0, (USB_SETUP_DEVICE_TO_HOST|USB_SETUP_TYPE_CLASS|USB_SETUP_RECIPIENT_INTERFACE), 0x03, last_bTag, 0x00, wInd, 0x0002, 0x0002, response, NULL);
    if (rcode)
    {
        pAsync->OnError(F("USBTMC Error on InitiateAbortBulkIn: rcode="), false);
        pAsync->OnError(String(rcode, HEX) + "h", true);
        return rcode;
    }

    *status = response[0];

    return rcode;
}

uint8_t USBTMC::CheckAbortBulkInStatus(uint8_t* status, uint8_t* bmAbortBulkIn)
{
    uint8_t rcode = 0;

    // USBTMC INITIATE ABORT BULKIN
    // bRequest = 0x04(4) CHECK ABORT BULKIN STATUS
    // wValLo = bTag.
    // wValHi = 0x00 Reserved. Must be 0x00.
    // total, nbytes = 0x0008
    uint8_t response[8];
    uint16_t wInd = 0x80 + epInfo[epDataInIndex].epAddr;
    rcode = pUsb->ctrlReq(bAddress, 0, (USB_SETUP_DEVICE_TO_HOST|USB_SETUP_TYPE_CLASS|USB_SETUP_RECIPIENT_ENDPOINT), 0x04, 0x00, 0x00, wInd, 0x0008, 0x0008, response, NULL);
    if (rcode)
    {
        pAsync->OnError(F("USBTMC Error on CheckAbortBulkInStatus: rcode="), false);
        pAsync->OnError(String(rcode, HEX) + "h", true);
        return rcode;
    }

    *status = response[0];
    *bmAbortBulkIn = response[1];

    return rcode;
}

void USBTMC::PrintEndpointDescriptor(const USB_ENDPOINT_DESCRIPTOR* ep_ptr)
{
    Notify(PSTR("Endpoint descriptor:"), 0x80);
    Notify(PSTR("\r\nLength:\t\t"), 0x80);
    D_PrintHex<uint8_t>(ep_ptr->bLength, 0x80);
    Notify(PSTR("\r\nType:\t\t"), 0x80);
    D_PrintHex<uint8_t>(ep_ptr->bDescriptorType, 0x80);
    Notify(PSTR("\r\nAddress:\t"), 0x80);
    D_PrintHex<uint8_t>(ep_ptr->bEndpointAddress, 0x80);
    Notify(PSTR("\r\nAttributes:\t"), 0x80);
    D_PrintHex<uint8_t>(ep_ptr->bmAttributes, 0x80);
    Notify(PSTR("\r\nMaxPktSize:\t"), 0x80);
    D_PrintHex<uint16_t>(ep_ptr->wMaxPacketSize, 0x80);
    Notify(PSTR("\r\nPoll Intrv:\t"), 0x80);
    D_PrintHex<uint8_t>(ep_ptr->bInterval, 0x80);
    Notify(PSTR("\r\n"), 0x80);
}
