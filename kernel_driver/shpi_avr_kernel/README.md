# shpi_avr_kernel
SHPI ATmega/ATtiny Kernel driver


in progress

## building

Build system and RPi kernel sources must be set up

```bash
# build module
git clone git@github.com:shpi/shpi_avr_kernel.git
cd shpi_avr_kernel
make
sudo dtc -I dts -O dtb -o /boot/overlays/shpi.dtbo shpi.dts

# load module
sudo dtoverlay /boot/overlays/shpi.dtbo
sudo modprobe backlight
sudo insmod shpi.ko
```

## backlight interface

/sys/class/backlight/2-002a $

actual_brightness   (read only 0-31)

bl_power            (0 on, 4 off)

brightness          (rw 0-31)

max_brightness      (31)


## led interface

/sys/class/leds/ws2818-red-0/brightness (LOGO red 0-255)

/sys/class/leds/ws2818-grn-0/brightness (LOGO gree 0-255)

/sys/class/leds/ws2818-blu-0/brightness (LOGO blue 0-255) 

/sys/class/leds/ws2818-red-1/brightness (SIGNAL red 0-255)

/sys/class/leds/ws2818-grn-1/brightness (SIGNAL green 0-255)

/sys/class/leds/ws2818-blu-1/brightness (SIGNAL blue 0-255)


## hwmon interface

/sys/class/hwmon/hwmon*/

buzzer

dfu_boot_enable

gasheater_enable

in1_input

in4_input

in6_input

in8_label

relay1

curr1_input

fan1_input

gasheater_enable_label

in2_input

in4_label

in7_input

name

pwm1

relay2

temp1_input

fw_version

in0_input

in3_input

in5_input

in8_input

pwm1_label

relay3

temp1_label

