"""
@file controller.py

@brief Basic interface with the Jtagger Arduino driver.
        Enables easy and convinient user interaction with the driver.

@author Michael Vigdorchik
"""

import serial
import sys
import struct
import time


# globals
INPUT_CHAR = ">"

# uart propreties
PORT = "COM4"  # or "/dev/ttyUSB0"
BAUD = 115200
TIMEOUT = 0.5  # sec


if len(sys.argv) < 3:
    print("Usage:")
    print("    python3 controller.py [COM4 for windows | /dev/ttyUSB0 for unix] 115200")
    sys.exit(0)


class Communicator():
    def __init__(self) -> None:
        self.s = serial.Serial(port=sys.argv[1],
                               baudrate=sys.argv[2],
                               timeout=TIMEOUT,
                               parity=serial.PARITY_NONE,
                               stopbits=serial.STOPBITS_ONE,
                               bytesize=serial.EIGHTBITS)
        self.s.flushInput()
        self.s.flushOutput()
    
    def writer(self, data: str):
        """
        Attempt to write data to serial device.
        @param data: String data to write.
        """
        if data == "":
            # user entered not data.
            # happens when just pressing "ENTER" key, to pass input function
            data = " "  # instead insert a SPACE character (0x20)
        self.s.write(bytes(data, "utf-8"))
        self.s.flush()

    def reader(self):
        """
        Attempt to read lines from serial device, till the INPUT_CHAR
        character is received.
        @return True if the INPUT_CHAR character is received, else
        return None if nothing was received. ("")
        """
        r = "0"
        while r != "":
            try:
                r = self.s.readline()  # read a '\n' terminated line or timeout
                r = r.decode("cp437")  # decodes utf-8 and more
                sys.stdout.write(r)
                sys.stdout.flush()
                self.s.flushInput()
                if INPUT_CHAR in r:  # user input is required
                    return True
            except serial.SerialTimeoutException as err:
                pass
        return None

    def init_arduino(self):
        """
        Read arduino "start message" and send a start signal.
        """
        # wait for arduino to send start message
        while self.reader() is None:
            pass
        # wait for user to send a start char to Arduino
        while input() != "s":
            pass
        self.writer("s")


def main():
    c = Communicator()
    c.init_arduino()
    
    # Entering Arduino's main menu
    while True:
        r = c.reader()
        if r is True:
            command = input()
        if command == "exit":
            break
        else:
            c.writer(command)
            c.reader()
    c.close()


if __name__ == "__main__":
    print(f"running {__name__}")
    main()
