import time
import serial

time.sleep(10)
with serial.Serial("/dev/tty.usbmodemHIDGD1", 9600, timeout=1) as ser:
    total = 30
    count = 0
    while count < total:
        ser.write(b"30\n")
        ser.flush()
        # exit()
        time.sleep(0.01)
        count += 1
    ser.write(b"0\n")
    ser.flush()