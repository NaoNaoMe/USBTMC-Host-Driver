# USBTMC HOST Driver
USBTMC (USB Test and Measurement Class) is a set of standard device class specifications, built on top of the USB standard.

This driver allows you to communicate with Measurement Instruments.
To use it you will need USB Host Shield 2.0, USB Host Shield Library, and some measurement instruments which have a USB Female Type B connector.

You can create own user interface or recall your favorite settings when you press a button on your Arduino board.

Some measurement instruments disabled remote control by default, so you need to enable USBTMC(or remote control) followed the instruction manual.

You can find a demonstration on my [youtube channel](https://youtu.be/e3xtPNQNZTE).

# Where is the USBTMC?
You can find it on your instrument's back panel.
The USB Type B device port offers instrument control.

![Example-back-panel](mdContents/back-panel-keysight-34465a.png)


# Example sketch
I wrote an example sketch that converts USBTMC to Serial communication.

The sketch shows up some USBTMC information(Capabilities) on serial monitor when you plug USBTMC devices into the USB Host Shield.

You type "*IDN?", press send button, then the USBTMC device responds an identifying string.

If you get a timeout message, the device doesn't support a command you typed.

![Example-serial-monitor](mdContents/SerialMonitorExample.gif)

This sketch is kind of stupid because it just makes slow communication.

Changing High-Speed(480Mbps) to Full-Speed(12Mbps), Communication with max3421e on SPI(8MHz), and Serial speed is 119200bps.

However, you don't need extra software on your PCs, you can just try to communicate with your instruments.
