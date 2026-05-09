import serial
import time

port = serial.Serial('/dev/ttyAMA0', baudrate=115200, timeout=1)
time.sleep(0.1)

test_str = b'hello from raspi\n'
port.write(test_str)
response = port.readline()

if response == test_str:
    print(f'PASS — received: {response}')
else:
    print(f'FAIL — sent: {test_str}, received: {response}')

port.close()