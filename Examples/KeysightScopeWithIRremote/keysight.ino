#define DECODE_NEC          // Includes Apple and Onkyo
#define IR_RECEIVE_PIN 2

#include <IRremote.hpp>

bool isResumed = false;
unsigned long previousMillis = 0;

void Initialize()
{
  IrReceiver.begin(IR_RECEIVE_PIN, DISABLE_LED_FEEDBACK);
  
}

void Routine()
{
  String command;
  String response;
  int request = 0;

  if (IrReceiver.decode()) {
    IrReceiver.resume();
    request = IrReceiver.decodedIRData.command;
  }

  if (Usb.getUsbTaskState() != USB_STATE_RUNNING) {
    return;
  }

  if(isResumed) {
    unsigned long currentMillis = millis();
    if (currentMillis - previousMillis < 200) {
      return;
    }
    isResumed = false;
  }

  if(request == 0x44){
      isResumed = true;
      previousMillis = millis();
      
      command = F(":OPERegister:CONDition?");
      worker.write(command);
      response = worker.read(1000);
      
      if(response == "") {
        return;
      }
      
      int value = response.toInt();
      value = value & (1 << 3); // Bit3 Name:Run Description:Running 
                                // When Set (1 = High = True), Indicates: The oscilloscope is running (not stopped).

      if(value == 0) {
        command = F(":RUN");
      }
      else {
        command = F(":STOP");
      }
      
      worker.write(command);
  }
  
  
}
