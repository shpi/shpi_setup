# WS2812 sysfs LED driver (SPI)
WS2812 sysfs LED kernel driver, pin 10 reused as input for PIR



## prerequisites

```bash
sudo apt-get install dkms git
```


## clone repository

```bash
git clone https://github.com/shpi/w2812_leds_sysfs
```

## install


```bash
cd w2812_leds_sysfs
sudo ln -s $HOME/w2812_leds_sysfs /usr/src/leds-ws2812-1.0
sudo dkms install leds-ws2812/1.0
sudo dtc -I dts -O dtb -o /boot/overlays/ws2812.dtbo ws2812.dts
```
