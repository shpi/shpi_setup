1. download buster lite image

https://downloads.raspberrypi.org/raspbian_lite_latest

infos how to flash sd card: https://www.raspberrypi.org/documentation/installation/installing-images/README.md


enable SSH:

create a "ssh" file in boot folder of sd card


setup WIFI:

Create a file in the root of boot called: wpa_supplicant.conf

Then copy the following code into it:

country=US
ctrl_interface=DIR=/var/run/wpa_supplicant GROUP=netdev
update_config=1

network={
    ssid="NETWORK-NAME"
    psk="NETWORK-PASSWORD"
}


Connect to your SHPI through SSH


sudo apt-get update

sudo apt-get upgrade

sudo apt-get install git

sudo apt-get install python3-pip

sudo apt-get install python3-pil

sudo apt-get install python3-numpy

sudo pip3 install RPi.GPIO

sudo pip3 install pi3d

sudo apt-get install librrd-dev libpython3-dev

sudo pip3 install rrdtool

sudo apt-get install python3-smbus

git clone https://github.com/shpi/zero_avr_firmware_std.git

git clone https://github.com/shpi/zero_std_setup.git

git clone https://github.com/shpi/zero_thermostat_demo.git

sudo apt-get install dfu-programmer

sudo apt-get install autotools-dev

sudo apt-get install automake

sudo apt-get install libusb-dev libusb-1.0-0 libusb-1.0-0-dev

sudo apt-get install gcc-avr binutils-avr avr-libc

sudo apt-get install wiringpi

sudo cp zero_std_setup/config.txt /boot/config.txt

sudo cp zero_std_setup/rc.local /etc/rc.local

cd zero_std_setup

sudo gcc -o hello hello.c -lbcm2835

optional:

crontab -e
@reboot python3 /home/pi/zero_thermostat_demo/demo.py

