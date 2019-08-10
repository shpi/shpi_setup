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

create a "ssh" file in boot folder of sd card


## 3. setup WIFI:

Create a file in the root of boot called: wpa_supplicant.conf

Then copy the following code into it:

```console
country=US

ctrl_interface=/var/run/wpa_supplicant GROUP=netdev

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

sudo apt-get install git omxplayer librrd-dev libpython3-dev python3-smbus python3-pip python3-pil python3-numpy dfu-programmer autotools-dev automake libusb-dev libusb-1.0-0 libusb-1.0-0-dev gcc-avr binutils-avr avr-libc wiringpi

sudo pip3 install RPi.GPIO pi3d rrdtool ics
```

Clone SHPI programs to your SHPI

```
git clone https://github.com/shpi/zero_other_demos.git

git clone https://github.com/shpi/zero_avr_firmware_std.git

git clone https://github.com/shpi/zero_std_setup.git

git clone https://github.com/shpi/zero_thermostat_demo.git
```

Copy necessary system files (for GPIO setup, i2c module and startup scripts)

```
sudo cp zero_std_setup/config.txt /boot/config.txt

sudo cp zero_std_setup/rc.local /etc/rc.local

sudo cp /home/pi/zero_std_setup/modules.conf /etc/modules-load.d/modules.conf
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

cd zero_std_setup

sudo gcc -o hello hello.c -lbcm2835

sudo gcc -o backlight backlight.c -lbcm2835

cp backlight /home/pi/zero_thermostat_demo/backlight

chmod +x /home/pi/zero_thermostat_demo/backlight
```

optional:

sudo crontab -e

insert line: 

```console
@reboot python3 /home/pi/zero_thermostat_demo/demo.py
```
