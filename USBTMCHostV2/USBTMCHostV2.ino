/*
 * Example sketch for the USBTMC Driver - developed by Naoya Imai
 */

#include <usbhub.h>

#include "usbtmc.h"

// Satisfy the IDE, which needs to see the include statement in the ino too.
#ifdef dobogusinclude
#include <spi4teensy3.h>
#endif

#include <SPI.h>

const char USB488Terminator = '\n';
const char SerialTerminator = '\n';

static bool isTransmitOnBin = false;

class USBTMCAsync : public USBTMCAsyncOper
{
  public:
    void OnRcvdDescr(USB* pUsb, USB_DEVICE_DESCRIPTOR* pdescr);
    void OnReceived(uint8_t data);
    void OnReadStatusByte(uint8_t status);
    void OnFailed(USBTMCInformation info, uint8_t code);
};

void USBTMCAsync::OnRcvdDescr(USB* pUsb, USB_DEVICE_DESCRIPTOR* pdescr)
{
  Serial.print(F("ProductID:"));
  Serial.println(pdescr->idProduct, HEX);

  Serial.print(F("VendorID:"));
  Serial.println(pdescr->idVendor, HEX);
  
}

void USBTMCAsync::OnReceived(uint8_t data)
{
    Serial.write(data);
}

void USBTMCAsync::OnReadStatusByte(uint8_t status)
{
    char high;
    char low;
    uint8_t tmp;
    tmp = (status >> 4) & 0x0F;
    if(tmp < 0x0A)
      high = tmp + 0x30;
    else
      high = (tmp - 0x0A) + 0x41;

    tmp = status & 0x0F;
    if(tmp < 0x0A)
      low = tmp + 0x30;
    else
      low = (tmp - 0x0A) + 0x41;

    String text = "0x";
    text += high;
    text += low;

    Serial.println(text);
}

void USBTMCAsync::OnFailed(USBTMCInformation info, uint8_t code)
{
    if(info == USBTMCInformation::ReceiveheaderNakAndTimeouted)
    {
        Serial.println(F("Receive timeout occured"));
    }
    else if(info == USBTMCInformation::ReceivepayloadNakAndTimeouted)
    {
        Serial.println(F("Receive timeout occured"));
    }
    else if(info == USBTMCInformation::AbortbulkinSucceed)
    {
        Serial.println(F("Abort Bulkin Succeed"));
    }
    else if(info == USBTMCInformation::ClaerSucceed)
    {
        Serial.println(F("Clear Succeed"));
    }
    else
    {
        Serial.print(F("USBTMCInformation = "));
        Serial.print(static_cast<int16_t>(info));

        if(info == USBTMCInformation::InitiateabortbulkoutFailed ||
           info == USBTMCInformation::InitiateabortbulkinFailed)
        {
            Serial.print(F(" USBTMC_status = "));
        }
        else
        {
            Serial.print(F(" rcode = "));
        }

        Serial.print(code, HEX);
        Serial.println(F("h"));
    }
   
}

USB Usb;
USBTMCAsync UsbtmcAsync;
USBTMC Usbtmc(&Usb, &UsbtmcAsync);

void setup()
{
    Serial.begin(115200);
#if !defined(__MIPSEL__)
    while (!Serial); // Wait for serial port to connect - used on Leonardo, Teensy and other boards with built-in USB CDC serial connection
#endif
    Serial.println(F("USBTMC Host Start"));

    if (Usb.Init() == -1)
        Serial.println(F("OSC did not start."));

    delay(200);

    Usbtmc.TimeStep(0); // Try to change timestep when you can not receive all of the data.
                        // Some test and measurement instruments can not respond quickly.
}

void loop()
{
    Usb.Task();

    if (Usb.getUsbTaskState() != USB_STATE_RUNNING)
    {
        Usbtmc.Run(false);
        return;
    }
    
    Usbtmc.Run(true);

    if(isTransmitOnBin)
    {
        // #48196XXXX,,,
        while (Serial.available() > 0)
        {
            Usbtmc.TransmitData(Serial.read());
            
            if(Usbtmc.TransmitDone())
            {
                isTransmitOnBin = false;
                break;
            }
        }
        
        return;
    }

    String receivedText = serialReceive();

    if(receivedText.length() < 4)
    {
        // invalid
        return;
    }

    String command = receivedText.substring(0, 4);
    String param = receivedText.substring(4);

    if(command == "##W;")
    {
        param += (char)USB488Terminator;

        Usbtmc.Transmit(param.length(), (uint8_t *)param.c_str());

    }
    else if(command == "##R;")
    {
        if(param == "")
            param = "1024";

        Usbtmc.Request(param.toInt());

    }
    else if(command == "#WB;")
    {
        // #WB;16
        // 16 represents 6 byte as 1 digits
        // #WB;41024
        // 41024 represents 1024 byte as 4 digits
        
        int digit = param.substring(0, 1).toInt();
        int rawDataCount = param.substring(1, 1+digit).toInt();

        Usbtmc.BeginTransmit(rawDataCount);
        
        isTransmitOnBin = true;

    }
    else if(command == "#RS;")
    {
        Usbtmc.ReadStatusByte();
    }
    else if(command == "##C;")
    {
        Usbtmc.Clear();
    }
    else
    {
        // ignore
    }
    
}

String serialReceive()
{
    static String tmpText = "";
    String receivedText = "";
    char rc;

    while (Serial.available() > 0)
    {
        rc = Serial.read();

        if ((rc == 0x00) || (!isAscii(rc)))
        {
            continue;
        }

        if (rc == SerialTerminator)
        {
            receivedText = tmpText;
            tmpText = "";
            break;
        }
        else
        {
            tmpText += rc;
        }
        
    }

    return receivedText;
}
