"""
@file controller.py

@brief Basic interface with the Jtagger Arduino driver.
        Enables easy and convinient user interaction with the driver.

@author Michael Vigdorchik
"""

import serial
import sys
import time
from serial.tools import list_ports


def list_available_ports() -> list:
    """List all available serial ports"""
    ports = list_ports.comports()
    if not ports:
        print("No serial ports found!")
        return []
    
    print("\nAvailable ports:")
    for i, port in enumerate(ports):
        print(f"{i + 1}: {port.device} - {port.description}")
    return ports


def get_user_port_selection(ports) -> str:
    """Get user's port selection"""
    while True:
        try:
            selection = int(input("\nSelect port number (or 0 to exit): ")) - 1
            if selection == -1:
                return None
            if 0 <= selection < len(ports):
                return ports[selection].device
            print("Invalid selection!")
        except ValueError:
            print("Please enter a valid number!")


# globals
INPUT_CHAR = ">"

# uart propreties
BAUD = 115200
TIMEOUT = 1  # sec


# if len(sys.argv) < 2:
#     print("Usage:")
#     print("    python3 controller.py [COM4 for Windows | /dev/ttyUSB0 for Unix | /dev/cu.usbmodem*some number* for MacOS]")
#     sys.exit(0)


class Communicator():
    def __init__(self, port) -> None:
        self.s = serial.Serial(
            port=port,
            baudrate=BAUD,
            bytesize=serial.EIGHTBITS,
            stopbits=serial.STOPBITS_ONE,
            parity=serial.PARITY_NONE,
            timeout=TIMEOUT
        )
        self.s.flushInput()
        self.s.flushOutput()
    
    def close(self):
        self.s.flush()
        self.s.close()
        print("\nSerial connection closed")

    def interact(self) -> bool:
        """
        Attempt to read lines from serial device, till the INPUT_CHAR
        character is received.
        When INPUT_CHAR is received, user needs to write an input to send.
        @return False user decided stop the serial interaction else return True.
        """
        r = ""  # read buffer
        w = ""  # write buffer
        while self.s.in_waiting:
            try:
                r = self.s.readline()  # read a '\n' terminated line or timeout
                r = r.decode("cp1252")  # decodes utf-8 and more (was cp437)
                sys.stdout.write(r)
                sys.stdout.flush()

                # user input is required - return
                if INPUT_CHAR in r:
                    w = (input() + '\n').encode()
                    self.s.write(w)
                    self.s.flush()
                    if w == b"z\n":
                        return False
                    else:
                        return True

            except serial.SerialException as error:
                print(f"Error: {error}")
        return True


def main():
    ports = list_available_ports()
    if not ports:
        return

    port = get_user_port_selection(ports)
    print(f"selected: {port}")
    if not port:
        return

    c = Communicator(port)
    while True:
        if not c.interact():
            break
        # Give Arduino time to respond
        time.sleep(0.1)
    c.close()


if __name__ == "__main__":
    main()
