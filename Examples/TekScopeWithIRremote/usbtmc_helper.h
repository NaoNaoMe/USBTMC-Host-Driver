/*
 * USBTMC class helper
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
#if !defined(__USBTMC_HELPER_H__)
#define __USBTMC_HELPER_H__

#include <usbhub.h>
#include "usbtmc.h"

class USBTMC_HELPER;

class USBTMC_HELPER : public USBTMC, public USBTMCAsyncOper
{
    USB *pUsb;
    USBTMCAsyncOper *pAsync;
    bool isRecieved;
    String receivedText;

    void OnRcvdDescr(USB_DEVICE_DESCRIPTOR *pdescr, uint8_t *serialNumPtr, uint8_t serialNumLen);
    void OnReceived(uint8_t data);
    void OnReadStatusByte(uint8_t status);
    void OnFailed(USBTMCInformation info, uint8_t code);

public:
    USBTMC_HELPER(USB *pusb, USBTMCAsyncOper *pasync,uint16_t vid = 0, uint16_t pid = 0);

    void write(String command);
    String read(unsigned long timeout);
    void task();
};

#endif // __USBTMC_HELPER_H__
