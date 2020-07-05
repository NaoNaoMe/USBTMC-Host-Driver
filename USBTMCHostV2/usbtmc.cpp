/*
 * USBTMC class driver for USB Host Shield 2.0 Library
 * Copyright (c) 2020 Naoya Imai
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

#define USBTMC_MESSAGE_SIZE 64
#define USBTMC_RCV_HEADER_SIZE 12

const uint8_t USBTMC::epDataInIndex = 1;
const uint8_t USBTMC::epDataOutIndex = 2;
const uint8_t USBTMC::epInterruptInIndex = 3;

USBTMC::USBTMC(USB* p, USBTMCAsyncOper * pasync) : pAsync(pasync), pUsb(p), bAddress(0), bNumEP(1), bTag(1), rtb_bTag(2), commandState(USBTMCState::Idle), bin_current_size(0), previousMillis(0), timestepMillis(0)
{
    fifo_flush();

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

    pAsync->OnRcvdDescr(pUsb, udd);


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

    rcode = GetCapabilities(&Capabilities);

    if (rcode)
        goto FailOnInit;

    // Does the interface accept REN_CONTROL request?
    if(Capabilities.USB488Interface & 0x02)
    {
        // USB488 REN_CONTROL
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

void USBTMC::Clear()
{
    commandState = USBTMCState::InitiateClear;
}

void USBTMC::Request(int length)
{
    uint8_t rcode = 0;

    if (!IsIdle())
    {
        pAsync->OnFailed(USBTMCInformation::RequestError, USBTMC_ERR_BUSY);
        return;
    }

    rcode = BulkOutRequest((uint32_t)length);

    if (rcode)
    {
        requestLength = 0;
        pAsync->OnFailed(USBTMCInformation::RequestError, rcode);
        return;
    }

    waitBeginMillis = millis();

    requestLength = (uint32_t)length;
    commandState = USBTMCState::ReceiveHeader;
}

void USBTMC::ReadStatusByte()
{
    uint8_t rcode = 0;

    // USB488 READ_STATUS_BYTE
    // bRequest = 0x80(128) READ_STATUS_BYTE
    // wValLo = bTag.
    // wValHi = 0x00 Reserved. Must be 0x00.
    // total, nbytes = 0x0003
    uint8_t response[3];
    uint16_t wInd = 0x0000;
    rcode = pUsb->ctrlReq(bAddress, 0, bmREQ_CL_GET_INTF, 0x80, rtb_bTag, 0x0000, wInd, 0x0003, 0x0003, response, NULL);
    last_rtb_bTag = rtb_bTag;
    if (rcode)
    {
        pAsync->OnFailed(USBTMCInformation::ReadstatusbyteError, rcode);
        return;
    }

    if (response[0] != 0x01)
    {
        pAsync->OnFailed(USBTMCInformation::ReadstatusbyteError, USBTMC_ERR_FAILED);
        return;
    }

    rtb_bTag++;
    if (rtb_bTag > 127)
        rtb_bTag = 2;

    if(Capabilities.USB488Interface & 0x02)
    {
        uint8_t status;

        rcode = ReadStatusByteFromInterruptEP(status, last_rtb_bTag);

        if (rcode)
        {
            pAsync->OnFailed(USBTMCInformation::ReadstatusbyteError, rcode);
        }
        else
        {
            pAsync->OnReadStatusByte(status);
        }

    }
    else
    {
        pAsync->OnReadStatusByte(response[2]);
    }

}

void USBTMC::Transmit(uint8_t nbytes, uint8_t* dataptr)
{
    if(!IsIdle())
    {
        pAsync->OnFailed(USBTMCInformation::TransmitError, USBTMC_ERR_BUSY);
        return;
    }

    BeginTransmit(nbytes);

    for(int i=0; i<nbytes; i++)
    {
        TransmitData(*dataptr++);
        if(TransmitDone())
            break;

    }

}

void USBTMC::BeginTransmit(uint32_t total_size)
{
    bin_total_size = total_size;
    bin_current_size = bin_total_size;
    isSentHeader = false;
}

void USBTMC::TransmitData(uint8_t data)
{
#define BUFFER_LENGTH 64

    uint8_t rcode = 0;

    if(fifo_available() >= (USBTMC_FIFO_SIZE-1))
    {
        fifo_flush();
        bin_current_size = 0;
        if(isSentHeader)
            commandState = USBTMCState::InitiateAbortBulkOut;
        isSentHeader = false;
        pAsync->OnFailed(USBTMCInformation::TransmitError, USBTMC_ERR_OVERFLOWED);
        return;
    }

    fifo_write(data);

    if(bin_current_size > 0)
        bin_current_size--;

    uint32_t max_packet_size;
    uint32_t remain;
    uint8_t buf[BUFFER_LENGTH];

    if(isSentHeader)
        max_packet_size = epInfo[epDataOutIndex].maxPktSize;
    else
        max_packet_size = epInfo[epDataOutIndex].maxPktSize - USBTMC_RCV_HEADER_SIZE;

    remain = (uint32_t)fifo_available();
    if(remain < max_packet_size)
        if(bin_current_size <= 0)
            max_packet_size = remain;
        else
            return;

    for(int i = 0; i < max_packet_size; i++)
        buf[i] = fifo_read();

    if(isSentHeader)
    {
        rcode = BulkOutData(max_packet_size, buf);
    }
    else
    {
        rcode = BulkOutData(max_packet_size, &buf[0], bin_total_size);
        if (!rcode)
            isSentHeader = true;
    }

    if (rcode)
    {
        fifo_flush();
        bin_current_size = 0;
        commandState = USBTMCState::InitiateAbortBulkOut;
        isSentHeader = false;
        if(isSentHeader)
            commandState = USBTMCState::InitiateAbortBulkOut;
        return;
    }

    if(bin_current_size <= 0)
    {
        fifo_flush();
        isSentHeader = false;
    }

#undef BUFFER_LENGTH
}

bool USBTMC::TransmitDone()
{
    if(bin_current_size <= 0)
        return true;
    else
        return false;
}

void USBTMC::AbortReceive()
{
    commandState = USBTMCState::InitiateAbortBulkIn;
}

void USBTMC::AbortTransmit()
{
    commandState = USBTMCState::InitiateAbortBulkOut;
}

void USBTMC::Run(bool isEnable)
{
#define BUFFER_LENGTH 64
    uint8_t rcode = 0;
    uint8_t status = 0;;
    uint8_t bmAbortBulkIn = 0;
    bool isFull = false;
    uint16_t rcvd = BUFFER_LENGTH;
    uint8_t buf[BUFFER_LENGTH];
    uint32_t currentMillis;

    USBTMCState state;

    if(isEnable == false)
    {
        commandState = USBTMCState::Idle;
        return;
    }

    currentMillis = millis();
    if ((currentMillis - previousMillis) < timestepMillis)
        return;

    previousMillis = currentMillis;

    switch (commandState)
    {
        case USBTMCState::Pause:
            if(isResume == false)
                commandState = resumedCommandState;

            break;

        case USBTMCState::ReceiveHeader:
            uint32_t totalLength;
            totalLength = requestLength;

            rcode = BulkIn(&rcvd, buf, totalLength);

            if (rcode == hrNAK)
            {
                //Try again
                currentMillis = millis();
                if ((currentMillis - waitBeginMillis) >= 5000)
                {
                    pAsync->OnFailed(USBTMCInformation::ReceiveheaderNakAndTimeouted, 0);
                    commandState = USBTMCState::InitiateAbortBulkIn;
                }

            }
            else if (rcode)
            {
                pAsync->OnFailed(USBTMCInformation::ReceiveheaderError, rcode);
                commandState = USBTMCState::Idle;
            }
            else
            {
                waitBeginMillis = millis();

                if(requestLength > totalLength)
                    requestLength = totalLength;

                if(rcvd > requestLength)
                    rcvd = requestLength;

                for (uint16_t i = USBTMC_RCV_HEADER_SIZE; i < (rcvd + USBTMC_RCV_HEADER_SIZE); i++)
                    pAsync->OnReceived(buf[i]);

                requestLength -= rcvd;

                if(requestLength > 0)
                    commandState = USBTMCState::ReceivePayload;
                else
                    commandState = USBTMCState::Idle;

            }

            break;

        case USBTMCState::ReceivePayload:

            rcode = BulkIn(&rcvd, buf);

            if (rcode == hrNAK)
            {
                //Try again
                currentMillis = millis();
                if ((currentMillis - waitBeginMillis) >= 5000)
                {
                    pAsync->OnFailed(USBTMCInformation::ReceivepayloadNakAndTimeouted, 0);
                    commandState = USBTMCState::InitiateAbortBulkIn;
                }

            }
            else if (rcode)
            {
                pAsync->OnFailed(USBTMCInformation::ReceivepayloadError, rcode);
                commandState = USBTMCState::Idle;
            }
            else
            {
                waitBeginMillis = millis();

                if(rcvd > requestLength)
                    rcvd = requestLength;

                for (uint16_t i = 0; i < rcvd; i++)
                    pAsync->OnReceived(buf[i]);

                requestLength -= rcvd;

                if(requestLength > 0)
                    commandState = USBTMCState::ReceivePayload;
                else
                    commandState = USBTMCState::Idle;

            }

            break;

        case USBTMCState::InitiateAbortBulkOut:
            rcode = InitiateAbortBulkOut(status);

            if (rcode)
            {
                pAsync->OnFailed(USBTMCInformation::InitiateabortbulkoutError, rcode);
                commandState = USBTMCState::Idle;
            }
            else
            {
                if(status == 0x01)  // STATUS_SUCCESS
                    commandState = USBTMCState::CheckAbortBulkOutStatus;
                else
                {
                    pAsync->OnFailed(USBTMCInformation::InitiateabortbulkoutFailed, status);
                    commandState = USBTMCState::Idle;
                }
            }

            break;

        case USBTMCState::CheckAbortBulkOutStatus:
            rcode = CheckAbortBulkOutStatus(status);

            if (rcode)
            {
                pAsync->OnFailed(USBTMCInformation::CheckabortbulkoutstatusError, rcode);
                commandState = USBTMCState::Idle;
            }
            else
            {
                if(status != 0x02)  // Not STATUS_PENDING
                    commandState = USBTMCState::ClearFeature;
            }

            break;

        case USBTMCState::InitiateAbortBulkIn:
            rcode = InitiateAbortBulkIn(status);

            if (rcode)
            {
                pAsync->OnFailed(USBTMCInformation::InitiateabortbulkinError, rcode);
                commandState = USBTMCState::Idle;
            }
            else
            {
                if(status == 0x01)  // STATUS_SUCCESS
                    commandState = USBTMCState::ReadingByAbortBulkIn;
                else
                {
                    pAsync->OnFailed(USBTMCInformation::InitiateabortbulkinFailed, status);
                    commandState = USBTMCState::Idle;
                }
            }

            break;

        case USBTMCState::ReadingByAbortBulkIn:
            rcode = PurgeBulkIn(isFull);

            if (rcode)
            {
                pAsync->OnFailed(USBTMCInformation::ReadingbyabortbulkinError, rcode);
                commandState = USBTMCState::Idle;
            }
            else
            {
                if(isFull)
                    commandState = USBTMCState::ReadingByAbortBulkIn;
                else
                    commandState = USBTMCState::CheckAbortBulkInStatus;
            }

            break;

        case USBTMCState::CheckAbortBulkInStatus:
            rcode = CheckAbortBulkInStatus(status, bmAbortBulkIn);

            if (rcode)
            {
                pAsync->OnFailed(USBTMCInformation::CheckabortbulkinstatusError, rcode);
                commandState = USBTMCState::Idle;
            }
            else
            {
                if(status != 0x02)  // Not STATUS_PENDING
                {
                    pAsync->OnFailed(USBTMCInformation::AbortbulkinSucceed, 0);
                    commandState = USBTMCState::Idle;
                 }
                else
                {
                    if(bmAbortBulkIn & 0x01 == 0x01)
                        commandState = USBTMCState::ReadingByAbortBulkIn;
                    else
                        commandState = USBTMCState::CheckAbortBulkInStatus;

                }
            }

            break;

        case USBTMCState::InitiateClear:
            rcode = InitiateClear(status);

            if (rcode)
            {
                pAsync->OnFailed(USBTMCInformation::InitiateclearError, rcode);
                commandState = USBTMCState::Idle;
            }
            else
            {
                if(status == 0x01)  // STATUS_SUCCESS
                    commandState = USBTMCState::CheckClearStatus;
                else
                {
                    pAsync->OnFailed(USBTMCInformation::InitiateclearFailed, status);
                    commandState = USBTMCState::Idle;
                }
            }

            break;

        case USBTMCState::CheckClearStatus:
            rcode = CheckClearStatus(status, bmAbortBulkIn);

            if (rcode)
            {
                pAsync->OnFailed(USBTMCInformation::CheckclearstatusError, rcode);
                commandState = USBTMCState::Idle;
            }
            else
            {
                if(status != 0x02)  // Not STATUS_PENDING
                    commandState = USBTMCState::ClearFeature;
                else
                {
                    if(bmAbortBulkIn & 0x01 == 0x01)
                        commandState = USBTMCState::ReadingByInitiateClear;
                    else
                        commandState = USBTMCState::CheckClearStatus;

                }
            }

            break;

        case USBTMCState::ReadingByInitiateClear:
            rcode = PurgeBulkIn(isFull);

            if (rcode)
            {
                pAsync->OnFailed(USBTMCInformation::ReadingbyinitiateclearError, rcode);
                commandState = USBTMCState::Idle;
            }
            else
            {
                if(isFull)
                    commandState = USBTMCState::ReadingByInitiateClear;
                else
                    commandState = USBTMCState::CheckClearStatus;
            }

            break;

        case USBTMCState::ClearFeature:
            // The Host must send a CLEAR_FEATURE request to clear the Bulk-OUT Halt.
            ClearFeature(epDataOutIndex);

            if (rcode)
                pAsync->OnFailed(USBTMCInformation::ClearfeatureError, rcode);
            else
                pAsync->OnFailed(USBTMCInformation::ClaerSucceed, 0);

            commandState = USBTMCState::Idle;

            break;

        default:
            break;
    }

    if(isResume == true)
    {
        if(commandState != USBTMCState::Idle && commandState != USBTMCState::Pause )
        {
            resumedCommandState = commandState;
            commandState = USBTMCState::Pause;
        }
        else
        {
            isResume = false;
        }
    }

#undef BUFFER_LENGTH
}

bool USBTMC::IsIdle()
{
    if (commandState == USBTMCState::Idle)
        return true;
    else
        return false;
}

bool USBTMC::IsPause()
{
    if (commandState == USBTMCState::Pause)
        return true;
    else
        return false;
}

void USBTMC::Pause()
{
    isResume = true;
}

void USBTMC::Unpause()
{
    isResume = false;
}

void USBTMC::TimeStep(uint32_t value)
{
    timestepMillis = value;
}

void USBTMC::EndpointXtract(uint8_t conf, uint8_t iface, uint8_t alt, uint8_t proto __attribute__((unused)), const USB_ENDPOINT_DESCRIPTOR* pep) {
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
}

uint8_t USBTMC::Release()
{
    uint8_t rcode = 0;

    pUsb->GetAddressPool().FreeAddress(bAddress);

    bAddress = 0;
    bNumEP = 1;
    return rcode;
}

uint8_t USBTMC::BulkOutData(uint8_t nbytes, uint8_t* dataptr, uint32_t totalbytes)
{
#define RESERVED_SIZE USBTMC_RCV_HEADER_SIZE
#define DEV_MESSAGE_BEGIN RESERVED_SIZE
    uint8_t message[USBTMC_MESSAGE_SIZE];
    uint16_t messageSize = RESERVED_SIZE;
    uint8_t rcode = 0;

    if (nbytes > (USBTMC_MESSAGE_SIZE - RESERVED_SIZE))
    {
        rcode = USBTMC_ERR_OVERFLOWED;
        return rcode;
    }

    messageSize += nbytes;

    if (messageSize > epInfo[epDataOutIndex].maxPktSize)
    {
        rcode = USBTMC_ERR_OVERFLOWED;
        return rcode;
    }

    //0:MsgID
    message[0] = 0x01; //DEV_DEP_MSG_OUT
    //1:bTag
    message[1] = bTag;
    //2:bTagInverse
    message[2] = ~bTag;
    //3:Reserved(0x00)
    message[3] = 0x00;
    //4,5,6,7:TransferSize
    message[4] = (uint8_t)(totalbytes       & 0x000000FF);
    message[5] = (uint8_t)(totalbytes >>  8 & 0x000000FF);
    message[6] = (uint8_t)(totalbytes >> 16 & 0x000000FF);
    message[7] = (uint8_t)(totalbytes >> 24 & 0x000000FF);
    //8:bmTransfer Attributes
    message[8] = 0x01; //(EOM is set)
    //9,10,11:Reserved(0x00)
    message[9] = 0x00;
    message[10] = 0x00;
    message[11] = 0x00;

    for (uint16_t i = DEV_MESSAGE_BEGIN; i < messageSize; i++)
        message[i] = *dataptr++;

    uint16_t quotient = messageSize / 4;

    if (messageSize > (quotient * 4))
        quotient++;

    rcode = pUsb->outTransfer(bAddress, epInfo[epDataOutIndex].epAddr, (quotient * 4), &message[0]);
    if (rcode)
        return rcode;

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

uint8_t USBTMC::BulkOutData(uint8_t nbytes, uint8_t* dataptr)
{
    uint8_t message[USBTMC_MESSAGE_SIZE];
    uint16_t messageSize = 0;
    uint8_t rcode = 0;

    if (nbytes > USBTMC_MESSAGE_SIZE)
    {
        rcode = USBTMC_ERR_OVERFLOWED;
        return rcode;
    }

    messageSize += nbytes;

    if (messageSize > epInfo[epDataOutIndex].maxPktSize)
    {
        rcode = USBTMC_ERR_OVERFLOWED;
        return rcode;
    }

    for (uint16_t i = 0; i < messageSize; i++)
        message[i] = *dataptr++;

    uint16_t quotient = messageSize / 4;

    if (messageSize > (quotient * 4))
        quotient++;

    rcode = pUsb->outTransfer(bAddress, epInfo[epDataOutIndex].epAddr, (quotient * 4), &message[0]);

    return rcode;

}

uint8_t USBTMC::BulkOutRequest(uint32_t nbytes)
{
#define MESSAGE_SIZE USBTMC_RCV_HEADER_SIZE
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
    message[4] = nbytes & 0xFF;
    message[5] = ( nbytes >>  8 )& 0xFF;
    message[6] = ( nbytes >> 16 )& 0xFF;
    message[7] = ( nbytes >> 24 )& 0xFF;
    //8:bmTransfer Attributes
    message[8] = 0x00; //D1 = 0 The device must ignore TermChar.
    //9:TermChar
    message[9] = 0x00; //If bmTransferAttributes.D1 = 0, the device must ignore this field.
    //10,11:Reserved(0x00)
    message[10] = 0x00;
    message[11] = 0x00;

    rcode = pUsb->outTransfer(bAddress, epInfo[epDataOutIndex].epAddr, messageSize, &message[0]);
    if (rcode)
        return rcode;

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


uint8_t USBTMC::BulkIn(uint16_t* bytes_rcvd, uint8_t* dataptr, uint32_t &length)
{
    uint16_t rcvd = *bytes_rcvd;
    uint8_t rcode = 0;

    rcode = pUsb->inTransfer(bAddress, epInfo[epDataInIndex].epAddr, &rcvd, dataptr);
    if (rcode)
    {
        *bytes_rcvd = 0;
        return rcode;
    }

    if (rcvd < USBTMC_RCV_HEADER_SIZE)
    {
        *bytes_rcvd = 0;
        rcode = USBTMC_ERR_UNEXPECTEDSIZE;
        return rcode;
    }

    uint32_t data_size;

    //4,5,6,7:TransferSize
    data_size = dataptr[7];
    data_size = data_size << 8;
    data_size += dataptr[6];
    data_size = data_size << 8;
    data_size += dataptr[5];
    data_size = data_size << 8;
    data_size += dataptr[4];

    length = data_size;

    *bytes_rcvd = rcvd - USBTMC_RCV_HEADER_SIZE;

    return rcode;
}

uint8_t USBTMC::BulkIn(uint16_t* bytes_rcvd, uint8_t* dataptr)
{
    uint8_t rcode = 0;

    rcode = pUsb->inTransfer(bAddress, epInfo[epDataInIndex].epAddr, bytes_rcvd, dataptr);

    return rcode;

}

uint8_t USBTMC::PurgeBulkIn(bool &isFull)
{
    uint8_t packet_size = epInfo[epDataInIndex].maxPktSize;
    uint8_t message[packet_size];
    uint16_t rcvd = packet_size;
    uint8_t rcode = 0;

    rcode = pUsb->inTransfer(bAddress, epInfo[epDataInIndex].epAddr, &rcvd, message);
    if (rcode)
        return rcode;

    if (rcvd >= packet_size)
        isFull = true;
    else
        isFull = false;

    return rcode;

}

uint8_t USBTMC::ReadStatusByteFromInterruptEP(uint8_t &status, uint8_t previous_btag)
{
    uint8_t rcode = 0;

    status = 0;

    uint8_t notify[2];
    uint16_t rcvd;
    rcvd = 2;

    rcode = pUsb->inTransfer(bAddress, epInfo[epInterruptInIndex].epAddr, &rcvd, notify);
    if (rcode)
        return rcode;

    if (rcvd != 2)
    {
        rcode = USBTMC_ERR_UNEXPECTEDSIZE;
        return rcode;
    }
    else
    {
        uint8_t number = notify[0];
        uint8_t res_btag = number & 0x7F;

        if ((number & 0x80) == 0x80 &&
            res_btag == previous_btag)           // The bTag value must be the same as the bTag value in the READ_STATUS_BYTE request.
        {
            status = notify[1];
        }

    }

    return rcode;
}

uint8_t USBTMC::InitiateAbortBulkOut(uint8_t &status)
{
    uint8_t rcode = 0;

    // USBTMC INITIATE ABORT BULKIN
    // bRequest = 0x01(1) Initiate Abort BulkOut
    // wValLo = bTag.
    // wValHi = 0x00 Reserved. Must be 0x00.
    // total, nbytes = 0x0002
    uint8_t response[2];
    uint16_t wInd = 0x80 + epInfo[epDataInIndex].epAddr;
    rcode = pUsb->ctrlReq(bAddress, 0, (USB_SETUP_DEVICE_TO_HOST|USB_SETUP_TYPE_CLASS|USB_SETUP_RECIPIENT_ENDPOINT), 0x01, last_bTag, 0x00, wInd, 0x0002, 0x0002, response, NULL);
    if (rcode)
        return rcode;

    status = response[0];

    return rcode;
}

uint8_t USBTMC::CheckAbortBulkOutStatus(uint8_t &status)
{
    uint8_t rcode = 0;

    // USBTMC INITIATE ABORT BULKIN
    // bRequest = 0x02(2) CHECK ABORT BULKOUT STATUS
    // wValLo = 0x00 Reserved. Must be 0x00.
    // wValHi = 0x00 Reserved. Must be 0x00.
    // total, nbytes = 0x0008
    uint8_t response[8];
    uint16_t wInd = 0x80 + epInfo[epDataInIndex].epAddr;
    rcode = pUsb->ctrlReq(bAddress, 0, (USB_SETUP_DEVICE_TO_HOST|USB_SETUP_TYPE_CLASS|USB_SETUP_RECIPIENT_ENDPOINT), 0x02, 0x00, 0x00, wInd, 0x0008, 0x0008, response, NULL);
    if (rcode)
        return rcode;

    status = response[0];

    return rcode;
}

uint8_t USBTMC::InitiateAbortBulkIn(uint8_t &status)
{
    uint8_t rcode = 0;

    // USBTMC INITIATE ABORT BULKIN
    // bRequest = 0x03(3) Initiate Abort BulkIn
    // wValLo = bTag.
    // wValHi = 0x00 Reserved. Must be 0x00.
    // total, nbytes = 0x0002
    uint8_t response[2];
    uint16_t wInd = 0x80 + epInfo[epDataInIndex].epAddr;
    rcode = pUsb->ctrlReq(bAddress, 0, (USB_SETUP_DEVICE_TO_HOST|USB_SETUP_TYPE_CLASS|USB_SETUP_RECIPIENT_ENDPOINT), 0x03, last_bTag, 0x00, wInd, 0x0002, 0x0002, response, NULL);
    if (rcode)
        return rcode;

    status = response[0];

    return rcode;
}

uint8_t USBTMC::CheckAbortBulkInStatus(uint8_t &status, uint8_t &bmAbortBulkIn)
{
    uint8_t rcode = 0;

    // USBTMC INITIATE ABORT BULKIN
    // bRequest = 0x04(4) CHECK ABORT BULKIN STATUS
    // wValLo = 0x00 Reserved. Must be 0x00.
    // wValHi = 0x00 Reserved. Must be 0x00.
    // total, nbytes = 0x0008
    uint8_t response[8];
    uint16_t wInd = 0x80 + epInfo[epDataInIndex].epAddr;
    rcode = pUsb->ctrlReq(bAddress, 0, (USB_SETUP_DEVICE_TO_HOST|USB_SETUP_TYPE_CLASS|USB_SETUP_RECIPIENT_ENDPOINT), 0x04, 0x00, 0x00, wInd, 0x0008, 0x0008, response, NULL);
    if (rcode)
        return rcode;

    status = response[0];
    bmAbortBulkIn = response[1];

    return rcode;
}

uint8_t USBTMC::InitiateClear(uint8_t &status)
{
    uint8_t rcode = 0;

    // USBTMC INITIATE CLEAR
    // bRequest = 0x05(5) Initiate Clear
    // wValLo = 0x00 Reserved. Must be 0x00.
    // wValHi = 0x00 Reserved. Must be 0x00.
    // total, nbytes = 0x0001
    uint8_t response[1];
    uint16_t wInd = 0x0000;
    rcode = pUsb->ctrlReq(bAddress, 0, bmREQ_CL_GET_INTF, 0x05, 0x00, 0x00, wInd, 0x0001, 0x0001, response, NULL);
    if (rcode)
        return rcode;

    status = response[0];

    return rcode;
}

uint8_t USBTMC::CheckClearStatus(uint8_t &status, uint8_t &bmAbortBulkIn)
{
    uint8_t rcode = 0;

    // USBTMC CHECK_CLEAR_STATUS
    // bRequest = 0x06(6) CHECK_CLEAR_STATUS
    // wValLo = 0x00 Reserved. Must be 0x00.
    // wValHi = 0x00 Reserved. Must be 0x00.
    // total, nbytes = 0x0002
    uint8_t response[2];
    uint16_t wInd = 0x0000;
    rcode = pUsb->ctrlReq(bAddress, 0, bmREQ_CL_GET_INTF, 0x06, 0x00, 0x00, wInd, 0x0002, 0x0002, response, NULL);
    if (rcode)
        return rcode;

    status = response[0];
    bmAbortBulkIn = response[1];

    return rcode;
}

uint8_t USBTMC::GetCapabilities(USBTMCCapabilities* pCapabilities)
{
    uint8_t rcode = 0;

    // USBTMC Get Capabilities
    // bRequest = 0x07(7) GET_CAPABILITIES
    // wValLo = 0x00
    // wValHi = 0x00
    // total, nbytes = 0x0018
    uint16_t wInd = 0x0000;
    return pUsb->ctrlReq(bAddress, 0, bmREQ_CL_GET_INTF, 0x07, 0x00, 0x00, wInd, 0x0018, 0x0018, (uint8_t*)pCapabilities, NULL);
}

uint8_t USBTMC::ClearFeature(uint8_t index)
{
    uint8_t rcode = 0;

    // CLEAR FEATURE
    // bRequest = Clear Feature
    // wVal as "Feature selector"
    // wValLo = 0x00(0) USB_FEATURE_ENDPOINT_HALT
    // wValHi = 0x00
    // total, nbytes = 0x0000
    uint16_t wInd = ((index == epDataInIndex) ? (0x80 | epInfo[index].epAddr) : epInfo[index].epAddr);
    rcode = pUsb->ctrlReq(bAddress, 0, (USB_SETUP_HOST_TO_DEVICE | USB_SETUP_TYPE_STANDARD | USB_SETUP_RECIPIENT_ENDPOINT), USB_REQUEST_CLEAR_FEATURE, USB_FEATURE_ENDPOINT_HALT, 0, wInd, 0, 0, NULL, NULL);

    if(rcode)
        return rcode;
    
    epInfo[index].bmSndToggle = 0;
    epInfo[index].bmRcvToggle = 0;

    return 0;
}

// long message fifo for transmit
uint16_t USBTMC::fifo_available()
{
    return ((uint16_t)(USBTMC_FIFO_SIZE + bin_fifo_buffer_head - bin_fifo_buffer_tail)) % USBTMC_FIFO_SIZE;
}

uint8_t USBTMC::fifo_peek()
{
    if (bin_fifo_buffer_head == bin_fifo_buffer_tail)
    {
        return -1;
    }
    else
    {
        return bin_fifo_buffer[bin_fifo_buffer_tail];
    }

}

uint8_t USBTMC::fifo_read()
{
    if (bin_fifo_buffer_head == bin_fifo_buffer_tail)
    {
        return -1;
    }
    else
    {
        uint8_t c = bin_fifo_buffer[bin_fifo_buffer_tail];
        bin_fifo_buffer_tail = (uint8_t)(bin_fifo_buffer_tail + 1) % USBTMC_FIFO_SIZE;
        return c;
    }

}

void USBTMC::fifo_write(uint8_t c)
{
    uint8_t i = (uint8_t)(bin_fifo_buffer_head + 1) % USBTMC_FIFO_SIZE;

    if (i != bin_fifo_buffer_tail) {
        bin_fifo_buffer[bin_fifo_buffer_head] = c;
        bin_fifo_buffer_head = i;
    }
}

void USBTMC::fifo_flush()
{
    bin_fifo_buffer_head = 0;
    bin_fifo_buffer_tail = 0;
}
