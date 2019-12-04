## Please note:


You must follow all instructions. The display remains dark until the file boot/config.txt is present
and the atmega is not started via "dfu-programmer". That's normal, no worries :-)


## 1. download buster lite image or regular image for dekstop

426mb  - https://downloads.raspberrypi.org/raspbian_lite_latest

1149mb -  https://downloads.raspberrypi.org/raspbian_latest


infos how to flash sd card: https://www.raspberrypi.org/documentation/installation/installing-images/README.md


username: pi

pasword: raspberry


## 2. enable SSH:

Enable SSH by placing a file named ssh (without extension), onto the boot partition of the SD card. The content of the file does not matter.

Raspbian will have two partitions. Place the ssh file on to the smaller boot partition.




## 3. setup WIFI:

Create a file in the root of boot called: wpa_supplicant.conf

Then copy the following code into it (replace NETWORK-NAME and NETWORK-PASSWORD with your WIFI credentials):

```console
country=US

ctrl_interface=DIR=/var/run/wpa_supplicant GROUP=netdev

update_config=1


network={

    ssid="NETWORK-NAME"

    psk="NETWORK-PASSWORD"

}
```

## 4. Connect to your SHPI through SSH and run following commands

```
sudo apt-get update

sudo apt-get upgrade

sudo apt-get install git sshpass libgles2-mesa libgles2-mesa-dev omxplayer librrd-dev libpython3-dev python3-smbus python3-pip python3-pil python3-numpy dfu-programmer autotools-dev automake libusb-dev libusb-1.0-0 libusb-1.0-0-dev gcc-avr binutils-avr avr-libc wiringpi nginx php-fpm

sudo pip3 install RPi.GPIO pi3d rrdtool ics pyowm icalendar 
```

Clone SHPI programs to your SHPI

```
git clone https://github.com/shpi/zero_other_demos.git

git clone https://github.com/shpi/zero_avr_firmware_std.git

git clone https://github.com/shpi/zero_setup.git

git clone https://github.com/shpi/zero_main_application.git
```

Copy necessary system files (for GPIO setup, i2c module and startup scripts)

```
sudo cp zero_setup/config.txt /boot/config.txt

sudo cp zero_setup/rc.local /etc/rc.local

sudo cp zero_setup/cmdline.txt /boot/cmdline.txt

sudo cp /home/pi/zero_setup/modules.conf /etc/modules-load.d/modules.conf

```

Compile and build bcm2835 c library for drivers

```
wget http://www.airspayce.com/mikem/bcm2835/bcm2835-1.60.tar.gz

tar zxvf bcm2835-1.60.tar.gz

cd bcm2835-1.60

./configure

make

sudo make check

sudo make install

cd ..

cd zero_setup

sudo gcc -o hello hello.c -lbcm2835

sudo gcc -o backlight backlight.c -lbcm2835

cp backlight /home/pi/zero_main_application/backlight

chmod +x /home/pi/zero_main_application/backlight
```

For usage of touchscreen in desktop run: zero_setup/touchdriver.py

Touchdriver is not necessary for main app.

optional:

sudo crontab -e

insert line: 

```console
@reboot python3 /home/pi/zero_main_application/main.py
```

## Zero Lite GPIO Configuration

init.c   - initialize lcd

gpio 10  is used for RGB LED and PIR, this works because the PIR goes high for several seconds if movement detected.

gpio 18  is fan (u can use PWM to control speed)

gpio 26  is touchinterrupt

gpio 19 is backlight control

gpio 27 is relais  1

gpio 11 is relais 2

gpio 1   is relais 3

i2c device 0x48 is ADS1015

ADS1015 alert can be used to control Buzzer

to control  RGB led use  -> https://github.com/jgarff/rpi_ws281x    and command ./test -g 10






