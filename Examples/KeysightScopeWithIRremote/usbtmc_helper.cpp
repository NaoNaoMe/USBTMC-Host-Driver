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
#include "usbtmc_helper.h"

USBTMC_HELPER::USBTMC_HELPER(USB *pusb, USBTMCAsyncOper * pasync, uint16_t vid, uint16_t pid) : USBTMC(pusb, this, vid, pid)
{
  pUsb = pusb;
  pAsync = pasync;
  isRecieved = false;
  receivedText = "";
}

void USBTMC_HELPER::OnRcvdDescr(USB_DEVICE_DESCRIPTOR *pdescr, uint8_t *serialNumPtr, uint8_t serialNumLen)
{
  pAsync->OnRcvdDescr(pdescr, serialNumPtr, serialNumLen);
}

void USBTMC_HELPER::OnReceived(uint8_t data)
{
  if (isRecieved)
  {
    return;
  }

  char rc = (char)data;
  if ((rc == 0x00) || (!isAscii(data))) {
    return;
  }
  
  if (rc == '\n') {
    isRecieved = true;
  }
  else {
    receivedText += rc;
  }
}

void USBTMC_HELPER::OnReadStatusByte(uint8_t status)
{
}

void USBTMC_HELPER::OnFailed(USBTMCInformation info, uint8_t code)
{
}

void USBTMC_HELPER::write(String command)
{
  if (pUsb->getUsbTaskState() != USB_STATE_RUNNING)
  {
    return;
  }
  command += '\n';
  Transmit(command.length(), (uint8_t *)command.c_str());
}

String USBTMC_HELPER::read(unsigned long timeout)
{
  String response = "";

  if (pUsb->getUsbTaskState() != USB_STATE_RUNNING)
  {
    return response;
  }

  unsigned long waitBeginMillis = millis();

  Request(1024);

  // sub loop function
  while (true)
  {
    if (pUsb->getUsbTaskState() != USB_STATE_RUNNING)
    {
      break;
    }

    task();

    unsigned long currentMillis = millis();
    if (currentMillis - waitBeginMillis >= timeout)
    {
      break;
    }

    if (isRecieved)
    {
      isRecieved = false;
      response = receivedText;
      receivedText = "";
      break;
    }
  }

  return response;
}

void USBTMC_HELPER::task()
{
  pUsb->Task();
  Run();
}
