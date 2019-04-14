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
#if !defined(__USBTMC_H__)
#define __USBTMC_H__

#include <Usb.h>

#define USBTMC_COMMAND_SIZE 64
#define USBTMC_REQUEST_SIZE 32

enum USBTMC_State { USBTMC_Request, USBTMC_Receive, USBTMC_Idle,
                    USBTMC_InitiateAbortBulkIn,
                    USBTMC_PurgingOnAbortBulkIn,
                    USBTMC_CheckAbortBulkInStatus };

typedef struct {
    uint8_t ReservedArray0[12];
    // See the USBTMC specification, Table 37. 488.2 USB488 interfaces
    // must set USBTMCInterfaceCapabilities.D1 = 0 and
    // USBTMCInterfaceCapabilities.D0 = 0.
    
    uint16_t bcdUSB488;
    // BCD version number of the relevant USB488 specification for this
    // USB488 interface. Format is as specified for bcdUSB in the USB 2.0
    // specification, section 9.6.1.
    
    uint8_t USB488Interface;
    // D7…D3 Reserved. All bits must be 0.
    // D2 1 – The interface is a 488.2 USB488 interface.
    //    0 – The interface is not a 488.2 USB488 interface.
    // D1 1 – The interface accepts REN_CONTROL,
    //        GO_TO_LOCAL, and LOCAL_LOCKOUT requests.
    //    0 – The interface does not accept REN_CONTROL,
    //        GO_TO_LOCAL, and LOCAL_LOCKOUT requests.
    //        The device, when REN_CONTROL,
    //        GO_TO_LOCAL, and LOCAL_LOCKOUT requests
    //        are received, must treat these commands as a nondefined
    //        command and return a STALL handshake
    //        packet.
    // D0 1 – The interface accepts the MsgID = TRIGGER
    //        USBTMC command message and forwards
    //        TRIGGER requests to the Function Layer.
    //    0 – The interface does not accept the TRIGGER
    //        USBTMC command message. The device, when the
    //        TRIGGER USBTMC command message is receives
    //        must treat it as an unknown MsgID and halt the
    //        Bulk-OUT endpoint.
    
    uint8_t USB488Device;
    // D7…D4 Reserved. All bits must be 0.
    // D3 1 – The device understands all mandatory SCPI
    //        commands. See SCPI Chapter 4, SCPI Compliance
    //        Criteria.
    //    0 – The device may not understand all mandatory SCPI
    //        commands. If the parser is dynamic and may not
    //        understand SCPI, this bit must = 0.
    // D2 1 – The device is SR1 capable. The interface must have
    //        an Interrupt-IN endpoint. The device must use the
    //        Interrupt-IN endpoint as described in 3.4.1 to
    //        request service, in addition to the other uses
    //        described in this specification.
    //    0 – The device is SR0. If the interface contains an
    //        Interrupt-IN endpoint, the device must not use the
    //        Interrupt-IN endpoint as described in 3.4.1 to
    //        request service. The device must use the endpoint
    //        for all other uses described in this specification.
    //        See IEEE 488.1, section 2.7. If USB488Interface
    //        Capabilities.D2 = 1, also see IEEE 488.2, section 5.5.
    // D1 1 – The device is RL1 capable. The device must
    //        implement the state machine shown in Figure 2.
    //    0 – The device is RL0. The device does not implement
    //        the state machine shown in Figure 2.
    //        See IEEE 488.1, section 2.8. If USB488Interface
    //        Capabilities.D2 = 1, also see IEEE 488.2, section 5.6.
    // D0 1 – The device is DT1 capable.
    //    0 – The device is DT0.
    //        See IEEE 488.1, section 2.11. If USB488Interface
    //        Capabilities.D2 = 1, also see IEEE 488.2, section 5.9.
    
    uint8_t ReservedArray1[8];
    // Reserved for USB488 use. All bytes must be 0x00.
    
} __attribute__((packed)) USBTMC_CAPABILITIES;

class USBTMC;

class USBTMCAsyncOper
{
public:
    virtual bool OnReceived(uint8_t data __attribute__((unused))) {
        return false;
    };
    
    virtual void OnError(String info, bool newline __attribute__((unused)));
};

// Only single port chips are currently supported by the library,
//              so only three endpoints are allocated.
#define USBTMC_MAX_ENDPOINTS    4

class USBTMC : public USBDeviceConfig, public UsbConfigXtracter {
    static const uint8_t epDataInIndex; // DataIn endpoint index
    static const uint8_t epDataOutIndex; // DataOUT endpoint index
    static const uint8_t epInterruptInIndex; // InterruptIN  endpoint index
    
    USBTMCAsyncOper* pAsync;
    USB* pUsb;
    uint8_t bAddress;
    uint8_t bConfNum; // configuration number
    uint8_t bNumIface; // number of interfaces in the configuration
    uint8_t bNumEP; // total number of EP in the configuration
    
    uint8_t last_bTag;
    uint8_t bTag;
    USBTMC_State CommandState;
    unsigned long WaitBeginMillis;
    
    EpInfo epInfo[USBTMC_MAX_ENDPOINTS];
    
    void PrintEndpointDescriptor(const USB_ENDPOINT_DESCRIPTOR* ep_ptr);
    
    uint8_t BulkOut_Data(uint8_t nbytes, uint8_t* dataptr);
    uint8_t BulkOut_Request(uint8_t nbytes);
    uint8_t BulkIn(uint16_t* bytes_rcvd, uint8_t* dataptr);
    uint8_t PurgeBulkIn(bool isFull);
    uint8_t InitiateAbortBulkIn(uint8_t* status);
    uint8_t CheckAbortBulkInStatus(uint8_t* status, uint8_t* bmAbortBulkIn);
    
public:
    USBTMC(USB* pusb, USBTMCAsyncOper* pasync);
    
    USBTMC_CAPABILITIES Capabilities;
    
    uint8_t Send(uint8_t nbytes, uint8_t* dataptr);
    void Run();
    bool IsIdle();
    
    // USBDeviceConfig implementation
    uint8_t Init(uint8_t parent, uint8_t port, bool lowspeed);
    uint8_t Release();
    virtual uint8_t GetAddress()
    {
        return bAddress;
    };
    
    // UsbConfigXtracter implementation
    void EndpointXtract(uint8_t conf, uint8_t iface, uint8_t alt, uint8_t proto, const USB_ENDPOINT_DESCRIPTOR* ep);
    
};

#endif // __USBTMC_H__
