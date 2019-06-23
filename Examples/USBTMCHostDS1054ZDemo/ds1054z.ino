// You can perform the following actions on your DS1054z
#define RUNSTOP
//#define CHANNEL
//#define MEASURE

bool isStop = false;
int channelNum = 1;

void RunInitialize()
{
    isStop = false;
    channelNum = 1;
    digitalWrite(PIN_LED, HIGH);
    
}

void RunScript()
{
#if defined RUNSTOP
    String command;
    if(isStop)
    {
        command = F(":RUN");
        isStop = false;
        digitalWrite(PIN_LED, HIGH);
    }
    else
    {
        command = F(":STOP");
        isStop = true;
        digitalWrite(PIN_LED, LOW);
    }

    sendCommand(command);

#elif defined CHANNEL
    String command;
    String range1;
    String range2;
    String range3;
    String range4;
    String edgeLevel;

    digitalWrite(PIN_LED, LOW);
    
    command = F(":CHAN1:DISP ON");
    sendCommand(command);

    command = F(":CHAN2:DISP ON");
    sendCommand(command);

    command = F(":CHAN3:DISP ON");
    sendCommand(command);

    command = F(":CHAN4:DISP ON");
    sendCommand(command);

    //:CHAN1:SCAL? -> 5.000000e+00
    command = F(":CHAN1:SCAL?");
    sendCommand(command);
    range1 = waitUntilReceived(5000);

    if(range1 == "")
        return;

    command = F(":CHAN2:SCAL?");
    sendCommand(command);
    range2 = waitUntilReceived(5000);

    if(range2 == "")
        return;

    command = F(":CHAN3:SCAL?");
    sendCommand(command);
    range3 = waitUntilReceived(5000);

    if(range3 == "")
        return;

    command = F(":CHAN4:SCAL?");
    sendCommand(command);
    range4 = waitUntilReceived(5000);

    if(range4 == "")
        return;

    command = F(":TRIG:EDG:LEV? ");
    sendCommand(command);
    edgeLevel = waitUntilReceived(5000);

    if(edgeLevel == "")
        return;

    command = F(":CHAN1:SCAL 10");
    sendCommand(command);

    command = F(":CHAN2:SCAL 10");
    sendCommand(command);

    command = F(":CHAN3:SCAL 10");
    sendCommand(command);

    command = F(":CHAN4:SCAL 10");
    sendCommand(command);

    command = F(":CHAN1:OFFS 20");
    sendCommand(command);

    command = F(":CHAN2:OFFS 0");
    sendCommand(command);

    command = F(":CHAN3:OFFS -20");
    sendCommand(command);

    command = F(":CHAN4:OFFS -40");
    sendCommand(command);

    command = F(":CHAN1:SCAL");
    command = command + " " + range1;
    sendCommand(command);

    command = F(":CHAN2:SCAL");
    command = command + " " + range2;
    sendCommand(command);

    command = F(":CHAN3:SCAL");
    command = command + " " + range3;
    sendCommand(command);

    command = F(":CHAN4:SCAL");
    command = command + " " + range4;
    sendCommand(command);

    command = F(":TRIG:EDG:LEV");
    command = command + " " + edgeLevel;
    sendCommand(command);

    digitalWrite(PIN_LED, HIGH);

#elif defined MEASURE
    String command;

    digitalWrite(PIN_LED, LOW);
    
    switch(channelNum)
    {
        case 1:
            command = F(":MEAS:SOUR CHAN1");
            channelNum = 2;
        break;
        
        case 2:
            command = F(":MEAS:SOUR CHAN2");
            channelNum = 3;
        break;
        
        case 3:
            command = F(":MEAS:SOUR CHAN3");
            channelNum = 4;
        break;
        
        case 4:
            command = F(":MEAS:SOUR CHAN4");
            channelNum = 1;
        break;

        defaule:
            command = F(":MEAS:SOUR CHAN1");
            channelNum = 1;
        break;
    }
    
    sendCommand(command);

    // Upload dummy settings -- begin
    command = F(":MEAS:ITEM VMAX");
    sendCommand(command);

    command = F(":MEAS:ITEM VMIN");
    sendCommand(command);

    command = F(":MEAS:ITEM VTOP");
    sendCommand(command);

    command = F(":MEAS:ITEM VAMP");
    sendCommand(command);
    // Upload dummy settings -- end

    // Upload actual settings
    command = F(":MEAS:ITEM VRMS");
    sendCommand(command);

    command = F(":MEAS:ITEM VAVG");
    sendCommand(command);

    command = F(":MEAS:ITEM VPP");
    sendCommand(command);

    command = F(":MEAS:ITEM FREQ");
    sendCommand(command);

    command = F(":MEAS:ITEM PER");
    sendCommand(command);

    digitalWrite(PIN_LED, HIGH);

#endif
    
}
