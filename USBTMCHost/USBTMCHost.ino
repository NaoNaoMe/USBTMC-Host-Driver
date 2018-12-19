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

const uint8_t USB488Terminator = 0x0A;
const char SerialTerminator = '\n';

class USBTMCAsync : public USBTMCAsyncOper
{
  public:
    bool OnReceived(uint8_t data);
    void OnError(String info);
};

bool USBTMCAsync::OnReceived(uint8_t data)
{
    if (data == USB488Terminator)
    {
        Serial.write((uint8_t)SerialTerminator);
        return true;
    }
    else
    {
        Serial.write(data);
        return false;
    }
}

void USBTMCAsync::OnError(String info)
{
    Serial.print(info);
    Serial.write((uint8_t)SerialTerminator);
}

USB Usb;
//USBHub         Hub(&Usb);
USBTMCAsync UsbtmcAsync;
USBTMC Usbtmc(&Usb, &UsbtmcAsync);

void setup()
{
    Serial.begin(115200);
#if !defined(__MIPSEL__)
    while (!Serial); // Wait for serial port to connect - used on Leonardo, Teensy and other boards with built-in USB CDC serial connection
#endif
    Serial.println("USBTMC Host Start");

    if (Usb.Init() == -1)
        Serial.println("OSC did not start.");

    delay(200);
}

void loop()
{
    static bool isFirstTime = false;

    Usb.Task();

    if (Usb.getUsbTaskState() != USB_STATE_RUNNING)
    {
        isFirstTime = false;
    }
    else
    {
        if (!isFirstTime)
        {
            isFirstTime = true;

            uint8_t interface = Usbtmc.Capabilities.USB488Interface;
            Serial.print("USB488Interface:");
            Serial.println(interface, HEX);
            if (interface & 0x04)
                Serial.println("The interface is a 488.2 USB488 interface.");
            else
                Serial.println("The interface is not a 488.2 USB488 interface.");

            if (interface & 0x02)
                Serial.println("The interface accepts REN_CONTROL, GO_TO_LOCAL, and LOCAL_LOCKOUT requests.");
            else
                Serial.println("The interface does not accept REN_CONTROL, GO_TO_LOCAL, and LOCAL_LOCKOUT requests.");

            if (interface & 0x01)
                Serial.println("The interface accepts the MsgID = TRIGGER USBTMC command message and forwards TRIGGER requests to the Function Layer.");
            else
                Serial.println("The interface does not accept the MsgID = TRIGGER USBTMC command message and forwards TRIGGER requests to the Function Layer.");

            uint8_t device = Usbtmc.Capabilities.USB488Device;
            Serial.print("USB488Device:");
            Serial.println(device, HEX);
            if (device & 0x08)
                Serial.println("The device understands all mandatory SCPI commands.");
            else
                Serial.println("The device may not understand all mandatory SCPI commands.");

            if (device & 0x04)
                Serial.println("The device is SR1 capable.");
            else
                Serial.println("The device is SR0.");

            if (device & 0x02)
                Serial.println("The device is RL1 capable.");
            else
                Serial.println("The device is RL0.");

            if (device & 0x01)
                Serial.println("The device is DT1 capable.");
            else
                Serial.println("The device is DT0.");
        }

        Usbtmc.Run();

        if (!Usbtmc.IsBlockRequest())
        {
            String receivedText = serialReceive();
            if(receivedText != "")
                Usbtmc.Send((uint8_t)receivedText.length(), (uint8_t *)receivedText.c_str());

        }

        delay(10);
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
            tmpText += (char)USB488Terminator;
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
