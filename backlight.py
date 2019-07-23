import time 
import sys,os 
import RPi.GPIO as GPIO 
GPIO.setmode(GPIO.BCM)


def write(a):
 GPIO.setup(10,GPIO.OUT)
 GPIO.output(10,GPIO.LOW)
 time.sleep(30/1000000.0)
 if a == 0:
  time.sleep(100/1000000.0)
 GPIO.setup(10,GPIO.IN)
 if a == 1:
  time.sleep(100/1000000.0)
 time.sleep(30/1000000.0)

 return

#setup backlight
GPIO.setup(10,GPIO.OUT)
GPIO.output(10,GPIO.LOW)
time.sleep(30000/1000000.0)
GPIO.setup(10,GPIO.IN)
time.sleep(150/1000000.0)
GPIO.setup(10,GPIO.OUT)
GPIO.output(10,GPIO.LOW)
time.sleep(500/1000000.0)
GPIO.setup(10,GPIO.IN)
time.sleep(500/1000000.0)
#setup backlight

answer = 0
for a in range(1,12):
 for i in range(1,28):
  brightness = [0,1,0,1,1,0,0,0,-1,1,0,0] +  [int(x) for x in list('{0:05b}'.format(i))] + [-1]
#  print(brightness)
  for x in  brightness:
        write(x)
  
  time.sleep(20/1000000.0)
  answer = answer + (GPIO.input(10))      
  time.sleep(0.01)

print(answer)  



