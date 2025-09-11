# Arduino JTAG/IEEE-1149.1 (1990) Standard Driver for Low Level Purposes

## Brief
Simple low level JTAG driver implemented on the Arduino platform.
Controlled by a python3 host via the Serial0 UART interface of the Arduino.

## Requirements
* pyserial3.5
* To build and flash: Use Arduino IDE that supports the DUE platform

## Notice
* Cannot be used on Arduino-Uno because it has not enough SRAM for the program to run.
* Use a different platform with more than 2KBytes of SRAM. (Use: Mega, Due ...)

# Future Work and Features
* JTAG / IEEE-1149.1 pinout detection --> Jtagulator style (or JTAGEnum)
* ARM SWD pinout detection            --> Jtagulator style
* UART pinout detection               --> Jtagulator style
* Find a way to detect multiple devices in chain
* Option to insert ir,dr to a specific device in chain
* Set verbosity options
* Wrap print functions ?
* Move function descriptions to header file