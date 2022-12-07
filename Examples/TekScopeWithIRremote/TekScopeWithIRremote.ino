/*
 * Example sketch for the Tektronix oscilloscopes - developed by Naoya Imai
 */

#include "usbtmc_helper.h"

class USBTMCHostAsyncOper : public USBTMCAsyncOper
{
public:
  void OnRcvdDescr(USB_DEVICE_DESCRIPTOR *pdescr, uint8_t *serialNumPtr, uint8_t serialNumLen){
    Serial.print(F("ProductID:"));
    Serial.println(pdescr->idProduct, HEX);
  
    Serial.print(F("VendorID:"));
    Serial.println(pdescr->idVendor, HEX);
  
    Serial.print(F("SerialNumber:"));
    for (int i = 2; i < serialNumLen; i += 2)
    { // string is UTF-16LE encoded
      Serial.print((char)serialNumPtr[i]);
    }
  
    Serial.println("");
  };
  void OnReceived(uint8_t data){};
  void OnReadStatusByte(uint8_t status){};
  void OnFailed(USBTMCInformation info, uint8_t code){};
};

USB Usb;
//USBHub Hub1(&Usb);
USBTMCHostAsyncOper  AsyncOper;
USBTMC_HELPER worker(&Usb,&AsyncOper);

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
#if !defined(__MIPSEL__)
  while (!Serial)
      ; // Wait for serial port to connect - used on Leonardo, Teensy and other boards with built-in USB CDC serial connection
#endif
  Serial.println(F("TekScope with IR remote"));

  if (Usb.Init() == -1)
    Serial.println(F("OSC did not start."));  

  delay(200);

  worker.TimeStep(0); // Try to change timestep when you can not receive all of the data.
                      // Some test and measurement instruments can not respond quickly.
  Initialize();
}

void loop() {
  // put your main code here, to run repeatedly:
  worker.task();
  Routine();
  
}
