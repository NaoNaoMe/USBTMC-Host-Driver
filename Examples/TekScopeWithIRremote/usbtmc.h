/*
 * USBTMC class driver for USB Host Shield 2.0 Library
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
#if !defined(__USBTMC_H__)
#define __USBTMC_H__

#include <Usb.h>

#define USBTMC_FIFO_SIZE 128

#define USBTMC_ERR_FAILED 0xF1
#define USBTMC_ERR_OVERFLOWED 0xF2
#define USBTMC_ERR_UNEXPECTEDSIZE 0xF3
#define USBTMC_ERR_BUSY 0xF4

enum class USBTMCState {
    Pause,
    ReceiveHeader,
    ReceivePayload,
    Idle,
    InitiateAbortBulkOut,
    CheckAbortBulkOutStatus,
    InitiateAbortBulkIn,
    ReadingByAbortBulkIn,
    CheckAbortBulkInStatus,
    InitiateClear,
    CheckClearStatus,
    ReadingByInitiateClear,
    ClearFeature
};

enum class USBTMCInformation : int16_t {
    AbortbulkinSucceed = 1,
    ClaerSucceed = 2,
    TransmitError = -1,
    RequestError = -2,
    ReadstatusbyteError = -3,
    ReceiveheaderNakAndTimeouted = -4,
    ReceiveheaderError = -5,
    ReceivepayloadNakAndTimeouted = -6,
    ReceivepayloadError = -7,
    InitiateabortbulkoutError = -8,
    InitiateabortbulkoutFailed = -9,
    CheckabortbulkoutstatusError = -10,
    InitiateabortbulkinError = -11,
    InitiateabortbulkinFailed = -12,
    ReadingbyabortbulkinError = -13,
    CheckabortbulkinstatusError = -14,
    InitiateclearError = -15,
    InitiateclearFailed = -16,
    CheckclearstatusError = -17,
    ReadingbyinitiateclearError = -18,
    ClearfeatureError = -19
};

typedef struct tagUSBTMC_CAPABILITIES {
    // GET_CAPABILITIES response on USBTMC Specification
    uint8_t USBTMC_status;

    uint8_t Reserved0;

    uint16_t bcdUSBTMC;
    // BCD version number of the relevant USBTMC specification for
    // this USBTMC interface. Format is as specified for bcdUSB in the
    // USB 2.0 specification, section 9.6.1.

    uint8_t USBTMCInterface;
    // D7-D3    Reserved. All bits must be 0.
    // D2 1     The USBTMC interface accepts the
    //          INDICATOR_PULSE request.
    //    0     The USBTMC interface does not accept the
    //          INDICATOR_PULSE request.The device, when
    //          an INDICATOR_PULSE request is received,
    //          must treat this command as a non-defined
    //          command and return a STALL handshake
    //          packet.
    // D1 1     The USBTMC interface is talk-only.
    //    0     The USBTMC interface is not talk-only.
    // D0 1     The USBTMC interface is listen-only.
    //    0     The USBTMC interface is not listen-only.

    uint8_t USBTMCDevice;
    // D7-D1    Reserved. All bits must be 0.
    // D0 1     The device supports ending a Bulk-IN transfer
    //          from this USBTMC interface when a byte
    //          matches a specified TermChar.
    //    0     The device does not support ending a Bulk-IN
    //          transfer from this USBTMC interface when a
    //          byte matches a specified TermChar.

    uint8_t ReservedArray0[6];

    // GET_CAPABILITIES response on Subclass USB488 Specification
    uint16_t bcdUSB488;
    // BCD version number of the relevant USB488 specification for this
    // USB488 interface. Format is as specified for bcdUSB in the USB 2.0
    // specification, section 9.6.1.

    uint8_t USB488Interface;
    // D7-D3 Reserved. All bits must be 0.
    // D2 1  The interface is a 488.2 USB488 interface.
    //    0  The interface is not a 488.2 USB488 interface.
    // D1 1  The interface accepts REN_CONTROL,
    //       GO_TO_LOCAL, and LOCAL_LOCKOUT requests.
    //    0  The interface does not accept REN_CONTROL,
    //       GO_TO_LOCAL, and LOCAL_LOCKOUT requests.
    //       The device, when REN_CONTROL,
    //       GO_TO_LOCAL, and LOCAL_LOCKOUT requests
    //       are received, must treat these commands as a nondefined
    //       command and return a STALL handshake
    //       packet.
    // D0 1  The interface accepts the MsgID = TRIGGER
    //       USBTMC command message and forwards
    //       TRIGGER requests to the Function Layer.
    //    0  The interface does not accept the TRIGGER
    //       USBTMC command message. The device, when the
    //       TRIGGER USBTMC command message is receives
    //       must treat it as an unknown MsgID and halt the
    //       Bulk-OUT endpoint.

    uint8_t USB488Device;
    // D7-D4 Reserved. All bits must be 0.
    // D3 1  The device understands all mandatory SCPI
    //       commands. See SCPI Chapter 4, SCPI Compliance
    //       Criteria.
    //    0  The device may not understand all mandatory SCPI
    //       commands. If the parser is dynamic and may not
    //       understand SCPI, this bit must = 0.
    // D2 1  The device is SR1 capable. The interface must have
    //       an Interrupt-IN endpoint. The device must use the
    //       Interrupt-IN endpoint as described in 3.4.1 to
    //       request service, in addition to the other uses
    //       described in this specification.
    //    0  The device is SR0. If the interface contains an
    //       Interrupt-IN endpoint, the device must not use the
    //       Interrupt-IN endpoint as described in 3.4.1 to
    //       request service. The device must use the endpoint
    //       for all other uses described in this specification.
    //       See IEEE 488.1, section 2.7. If USB488Interface
    //       Capabilities.D2 = 1, also see IEEE 488.2, section 5.5.
    // D1 1  The device is RL1 capable. The device must
    //       implement the state machine shown in Figure 2.
    //    0  The device is RL0. The device does not implement
    //       the state machine shown in Figure 2.
    //       See IEEE 488.1, section 2.8. If USB488Interface
    //       Capabilities.D2 = 1, also see IEEE 488.2, section 5.6.
    // D0 1  The device is DT1 capable.
    //    0  The device is DT0.
    //       See IEEE 488.1, section 2.11. If USB488Interface
    //       Capabilities.D2 = 1, also see IEEE 488.2, section 5.9.

    uint8_t ReservedArray1[8];
    // Reserved for USB488 use. All bytes must be 0x00.

} __attribute__((packed)) USBTMCCapabilities;

class USBTMC;

class USBTMCAsyncOper
{
public:
    virtual void OnRcvdDescr(USB_DEVICE_DESCRIPTOR *pdescr, uint8_t *serialNumPtr __attribute__((unused)), uint8_t serialNumLen __attribute__((unused)));

    virtual void OnReceived(uint8_t data);

    virtual void OnReadStatusByte(uint8_t status);

    virtual void OnFailed(USBTMCInformation info, uint8_t code);
};

// Only single port chips are currently supported by the library,
//              so only three endpoints are allocated.
#define USBTMC_MAX_ENDPOINTS 4

class USBTMC : public USBDeviceConfig, public UsbConfigXtracter {
    static const uint8_t epDataInIndex;      // DataIn endpoint index
    static const uint8_t epDataOutIndex;     // DataOUT endpoint index
    static const uint8_t epInterruptInIndex; // InterruptIN  endpoint index

    USBTMCAsyncOper *pAsync;
    USB *pUsb;
    uint8_t bAddress;
    uint8_t bConfNum;  // configuration number
    uint8_t bNumIface; // number of interfaces in the configuration
    uint8_t bNumEP;    // total number of EP in the configuration
    bool isConnected;
    uint16_t targetVID;
    uint16_t targetPID;
    const uint8_t *serialNumberDataPtr;

    uint8_t last_bTag;
    uint8_t bTag;
    uint8_t last_rtb_bTag;
    uint8_t rtb_bTag;
    USBTMCState commandState;
    USBTMCState resumedCommandState;
    uint32_t waitBeginMillis;
    uint32_t previousMillis;
    uint32_t timestepMillis;
    int requestLength;

    EpInfo epInfo[USBTMC_MAX_ENDPOINTS];

    volatile uint8_t bin_fifo_buffer_head;
    volatile uint8_t bin_fifo_buffer_tail;
    uint8_t bin_fifo_buffer[USBTMC_FIFO_SIZE];

    uint32_t bin_total_size;
    uint32_t bin_current_size;

    bool isSentHeader;
    bool isResume;

    uint8_t GetStringDescriptor(uint8_t addr, uint8_t idx, uint8_t *dataptr, uint8_t *length);

    uint8_t InitiateAbortBulkOut(uint8_t &status);
    uint8_t CheckAbortBulkOutStatus(uint8_t &status);
    uint8_t InitiateAbortBulkIn(uint8_t &status);
    uint8_t CheckAbortBulkInStatus(uint8_t &status, uint8_t &bmAbortBulkIn);
    uint8_t InitiateClear(uint8_t &status);
    uint8_t CheckClearStatus(uint8_t &status, uint8_t &bmAbortBulkIn);
    uint8_t GetCapabilities(USBTMCCapabilities *pCapabilities);

    uint8_t PurgeBulkIn(bool &isFull);

    uint8_t ClearFeature(uint8_t index);

    uint8_t BulkOutData(uint8_t nbytes, uint8_t *dataptr, uint32_t totalbytes);
    uint8_t BulkOutData(uint8_t nbytes, uint8_t *dataptr);
    uint8_t BulkOutRequest(uint32_t nbytes);
    uint8_t BulkIn(uint16_t *bytes_rcvd, uint8_t *dataptr, uint32_t &length);
    uint8_t BulkIn(uint16_t *bytes_rcvd, uint8_t *dataptr);

    uint8_t ReadStatusByteFromInterruptEP(uint8_t &status, uint8_t previous_btag);

    uint16_t fifo_available();
    uint8_t fifo_peek();
    uint8_t fifo_read();
    void fifo_write(uint8_t c);
    void fifo_flush();

public:
    USBTMC(USB *pusb, USBTMCAsyncOper *pasync, uint16_t vid = 0, uint16_t pid = 0);

    USBTMCCapabilities Capabilities;
    bool IsConnected();
    void SetTargetSerialNumber(const uint8_t *serialNumPtr);

    void Clear();
    void Request(int length);
    void ReadStatusByte();

    void Transmit(uint8_t nbytes, uint8_t *dataptr);
    void BeginTransmit(uint32_t total_size);
    void TransmitData(uint8_t data);
    bool TransmitDone();

    void AbortReceive();
    void AbortTransmit();

    void Run();
    bool IsIdle();
    bool IsPause();

    void Pause();
    void Unpause();

    void TimeStep(uint32_t value);

    // USBDeviceConfig implementation
    uint8_t Init(uint8_t parent, uint8_t port, bool lowspeed);
    uint8_t Release();
    uint8_t GetAddress() {
        return bAddress;
    };

    // UsbConfigXtracter implementation
    void EndpointXtract(uint8_t conf, uint8_t iface, uint8_t alt, uint8_t proto, const USB_ENDPOINT_DESCRIPTOR *ep);
	
};

#endif // __USBTMC_H__
