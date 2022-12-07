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

  switch(request){
    case 0x44:
      isResumed = true;
      previousMillis = millis();
      
      command = F("ACQ:STATE?");
      worker.write(command);
      response = worker.read(1000);
      
      if(response == "") {
        return;
      }
      
      if(response == "0") {
        command = F("ACQ:STATE RUN");
      }
      else {
        command = F("ACQ:STATE STOP");
      }
      
      worker.write(command);
    break;

    default:
    break;
  }
  
  
}
