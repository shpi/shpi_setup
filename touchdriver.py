#!/usr/bin/env python

import os
import signal
import sys
import time


try:
    from evdev import uinput, UInput, AbsInfo, ecodes as e
except ImportError:
    exit("Install: sudo pip install evdev")

try:
    import RPi.GPIO as gpio
except ImportError:
    exit("Install: sudo pip install RPi.GPIO")

try:
    import smbus
except ImportError:
    exit("Install: sudo apt-get install python-smbus")


os.system("sudo modprobe uinput")

CAPABILITIES = {
    e.EV_ABS : (
        (e.ABS_X, AbsInfo(value=0, min=0, max=800, fuzz=0, flat=0, resolution=1)),
        (e.ABS_Y, AbsInfo(value=0, min=0, max=480, fuzz=0, flat=0, resolution=1)),
    ),
    e.EV_KEY : [
        e.BTN_TOUCH, 
    ]
}

PIDFILE = "/var/run/touch.pid"


try:
        pid = os.fork()
        if pid > 0:
            sys.exit(0)

except OSError as  e:
        print("Fork #1 failed: {} ({})".format(e.errno, e.strerror))
        sys.exit(1)


os.chdir("/")
os.setsid()
os.umask(0)

try:
        pid = os.fork()
        if pid > 0:
            fpid = open(PIDFILE, 'w')
            fpid.write(str(pid))
            fpid.close()
            sys.exit(0)

except OSError as e:

        print("Fork #2 failed: {} ({})".format(e.errno, e.strerror))
        sys.exit(1)


#si = file("/dev/null", 'r')
#so = file("/dev/null", 'a+', 0)
#se = file("/dev/null", 'a+', 0)

#os.dup2(si.fileno(), sys.stdin.fileno())
#os.dup2(so.fileno(), sys.stdout.fileno())
#os.dup2(se.fileno(), sys.stderr.fileno())

try:
    ui = UInput(CAPABILITIES, name="Touchscreen", bustype=e.BUS_USB)

except uinput.UInputError as e:
    sys.stdout.write(e.message)
    sys.stdout.write("Running as root? sudo ...".format(sys.argv[0]))
    sys.exit(0)

INT = 26
ADDR = 0x5c

gpio.setmode(gpio.BCM)
gpio.setwarnings(False)
gpio.setup(INT, gpio.IN)

bus = smbus.SMBus(2)


start = 0



def touchint(channel):
  global start
  if gpio.input(channel):
     start = 1
  else:
      ui.write(e.EV_KEY, e.BTN_TOUCH, 0)
      ui.write(e.EV_SYN, 0, 0)
      ui.syn()
      print('released')

gpio.add_event_detect(INT, gpio.BOTH, callback=touchint)      #touch interrupt


def write_status(x, y):
            global start
            if start:
              print('touched')
              ui.write(e.EV_KEY, e.BTN_TOUCH, 1)
              start = 0            

            ui.write(e.EV_ABS, e.ABS_X, x)
            ui.write(e.EV_ABS, e.ABS_Y, y)
  
            ui.write(e.EV_SYN, 0, 0)
            ui.syn()


xc, yc = 0, 0

def smbus_read_touch():
    global xc,yc    

    try:
        data = bus.read_i2c_block_data(ADDR, 0x40, 8)

        x = data[0] | (data[4] << 8)
        y = data[1] | (data[5] << 8)
        
        if (0 < x < 800)  and (0 < y < 480):
          if ((-80 < (xc-x) < 80) & (-80 < (yc-y) < 80)):  #catch bounches
            xc, yc = x, y
            write_status(800-x, 480-y)
            print(800-xc,480-yc)
          elif 801 > x > 0:
            xc, yc = x,y
            time.sleep(0.02)          
            smbus_read_touch()
    except:
        pass

bus.write_byte_data(0x5c,0x6e,0b00001110)

while True:
        if gpio.input(INT):
            smbus_read_touch()
            time.sleep(0.02)
        time.sleep(0.005)
ui.close()

