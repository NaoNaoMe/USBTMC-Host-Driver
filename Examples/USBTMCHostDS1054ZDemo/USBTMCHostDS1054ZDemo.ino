/*
 * Example sketch for the USBTMC Driver - developed by Naoya Imai
 */

#include <usbhub.h>

// Please copy usbtmc.cpp and usbtmc.h to "USBTMCHostDS1054ZDemo" folder.
#include "usbtmc.h"

// Satisfy the IDE, which needs to see the include statement in the ino too.
#ifdef dobogusinclude
#include <spi4teensy3.h>
#endif

#include <SPI.h>

#define PIN_LED     3
#define PIN_BUTTON  2

const uint8_t USB488Terminator = 0x0A;
const char SerialTerminator = '\n';

String ReceivedText;
bool isReceived = false;

bool isFired = false;
unsigned long beginMillis;

class USBTMCAsync : public USBTMCAsyncOper
{
  public:
    bool OnReceived(uint8_t data);
    void OnError(String info, bool newline);
};

bool USBTMCAsync::OnReceived(uint8_t data)
{
    if (data == USB488Terminator)
    {
        Serial.write((uint8_t)SerialTerminator);
        isReceived = true;
        return true;
    }
    else
    {
        Serial.write(data);
        ReceivedText += (char)data;
        return false;
    }
}

void USBTMCAsync::OnError(String info, bool newline)
{
    Serial.print(info);
    if(newline)
        Serial.write((uint8_t)SerialTerminator);
}

USB Usb;
//USBHub         Hub(&Usb);
USBTMCAsync UsbtmcAsync;
USBTMC Usbtmc(&Usb, &UsbtmcAsync);

void sendCommand(String command)
{
    Serial.println(command);
    command += (char)USB488Terminator;
    Usbtmc.Send((uint8_t)command.length(), (uint8_t *)command.c_str());
}

String waitUntilReceived(unsigned long timeout)
{
    ReceivedText = "";
    isReceived = false;
    unsigned long waitBeginMillis = millis();
  
    // sub loop function
    while(true)
    {
        unsigned long currentMillis = millis();
        if (currentMillis - waitBeginMillis >= timeout)
        {
            ReceivedText = "";
            break;
        }
        
        Usb.Task();

        if (Usb.getUsbTaskState() != USB_STATE_RUNNING)
        {
            ReceivedText = "";
            break;
        }
        else
        {
            Usbtmc.Run();

            if(isReceived)
                break;

        }

    }

    return ReceivedText;
}

void setup()
{
    digitalWrite(PIN_LED, LOW);
    pinMode(PIN_LED, OUTPUT);
    pinMode(PIN_BUTTON, INPUT);
  
    Serial.begin(115200);
#if !defined(__MIPSEL__)
    while (!Serial); // Wait for serial port to connect - used on Leonardo, Teensy and other boards with built-in USB CDC serial connection
#endif
    Serial.println(F("USBTMC Host DS1054Z Demonstration"));

    if (Usb.Init() == -1)
        Serial.println(F("OSC did not start."));

    delay(200);
    
    beginMillis = millis();
}

void loop()
{
    Usb.Task();

    if (Usb.getUsbTaskState() != USB_STATE_RUNNING)
    {
        RunInitialize();
    }
    else
    {
        Usbtmc.Run();

        if (Usbtmc.IsIdle())
        {
            String receivedText = serialReceive();
            if(receivedText != "")
                Usbtmc.Send((uint8_t)receivedText.length(), (uint8_t *)receivedText.c_str());

        }

        int inPin = digitalRead(PIN_BUTTON);
        if (inPin == LOW) {
          unsigned long currentMillis = millis();
          if (currentMillis - beginMillis >= 100) {
            if(!isFired) {
              isFired = true;
              inPin = LOW;
            }
            else {
              inPin = HIGH;          
            }
            
          }
          else {
            inPin = HIGH;
          }
        }
        else {
          beginMillis = millis();
          isFired = false;
        }

        if (inPin == LOW) {
            RunScript();
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
