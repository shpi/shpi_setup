# SHPI AVR Kernel driver

SHPI atmega32u4 kernal driver on address 0x2A


## prerequisites

```bash
sudo apt-get install dkms git
```


## install


```bash
cd $HOME/shpi_setup/kernel_driver/shpi_avr_kernel 
sudo ln -s $HOME/shpi_setup/kernel_driver/shpi_avr_kernel /usr/src/shpi-1.0
sudo dkms install shpi/1.0
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

