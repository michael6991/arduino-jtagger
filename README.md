# Arduino JTAG/IEEE-1149.1 (1990) Standard Driver for Low Level Purposes

## Brief
Simple low level JTAG driver implemented on the Arduino platform.
Controlled by a python3 host via the Serial0 UART interface of the Arduino.

## Requirements
* pyserial
* Arduino IDE that supports the DUE platform

## Notice
* Cannot be used on Arduino-Uno because it has not enough SRAM for the program to run.
* Use a different platform with more than 2KBytes of SRAM. (Use: Mega, Due ...)

# Future Work and Features
* JTAG / IEEE-1149.1 pinout detection
* ARM SWD pinout detection
* UART pinout detection

<!-- Additional Tasks -->
* implement the jtagulator algorithm
* find a way to detect multiple devices in chain
* option to insert ir,dr to a specific device in chain
