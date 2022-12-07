/*
 * USB Modular instruments unlocker class for USB Host Shield 2.0 Library
 * Copyright (c) 2022 Naoya Imai
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
#include "unlocker.h"

UNLOCKER::UNLOCKER(USB *p) : pUsb(p), bAddress(0)
{
    for (uint8_t i = 0; i < USBTMC_MAX_ENDPOINTS; i++)
    {
        epInfo[i].epAddr = 0;
        epInfo[i].maxPktSize = (i) ? 0 : 8;
        epInfo[i].bmSndToggle = 0;
        epInfo[i].bmRcvToggle = 0;
        epInfo[i].bmNakPower = USB_NAK_MAX_POWER;
    }

    if (pUsb)
        pUsb->RegisterDeviceClass(this);
}

uint8_t UNLOCKER::Init(uint8_t parent, uint8_t port, bool lowspeed)
{
    const uint8_t constBufSize = sizeof(USB_DEVICE_DESCRIPTOR);

    uint8_t buf[constBufSize];
    USB_DEVICE_DESCRIPTOR *udd = reinterpret_cast<USB_DEVICE_DESCRIPTOR *>(buf);
    uint8_t rcode;
    UsbDevice *p = NULL;
    EpInfo *oldep_ptr = NULL;

    AddressPool &addrPool = pUsb->GetAddressPool();

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

    // VID: 0x0957 Agilent Technologies, Inc.
    if (udd->idVendor != 0x0957)
    {
        rcode = USB_DEV_CONFIG_ERROR_DEVICE_NOT_SUPPORTED;
        goto FailOnInit;
    }

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

    // Assign epInfo to epinfo pointer
    rcode = pUsb->setEpInfoEntry(bAddress, 1, epInfo);

    if (rcode)
        goto FailSetDevTblEntry;

    if (udd->bNumConfigurations != 1)
        goto FailSetDevTblEntry;

    ConfigDescParser<0x00, 0x00, 0x00, CP_MASK_COMPARE_ALL> confDescrParser(this);

    rcode = pUsb->getConfDescr(bAddress, 0, 0, &confDescrParser);

    if (rcode)
        goto FailGetConfDescr;

    // Assign epInfo to epinfo pointer
    rcode = pUsb->setEpInfoEntry(bAddress, 1, epInfo);

    // Set Configuration Value
    rcode = pUsb->setConf(bAddress, 0, 1);

    if (rcode)
        goto FailSetConfDescr;

    // Change to USBTMC device from the initial state
    uint8_t buffer[] = {0x00, 0x00, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00};
    rcode = pUsb->ctrlReq(bAddress, 0, (USB_SETUP_HOST_TO_DEVICE | USB_SETUP_TYPE_VENDOR | USB_SETUP_RECIPIENT_DEVICE), 0x0C, 0x00, 0x00, 0x0475, 0x0008, 0x0008, &buffer[0], NULL);

    if (rcode)
        goto FailOnInit;

    return 0;

FailGetDevDescr:
#ifdef DEBUG_USB_HOST
    NotifyFailGetDevDescr();
    goto Fail;
#endif

FailSetDevTblEntry:
#ifdef DEBUG_USB_HOST
    NotifyFailSetDevTblEntry();
    goto Fail;
#endif

FailGetConfDescr:
#ifdef DEBUG_USB_HOST
    NotifyFailGetConfDescr();
    goto Fail;
#endif

FailSetConfDescr:
#ifdef DEBUG_USB_HOST
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

uint8_t UNLOCKER::Release()
{
    uint8_t rcode = 0;

    pUsb->GetAddressPool().FreeAddress(bAddress);

    bAddress = 0;
    return rcode;
}
