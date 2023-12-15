# cyc8c2-touchdriver
A035VW01 kernel touchscreen driver, interrupt on gpio 26, i2c address 0x5c




## prerequisites

```bash
sudo apt-get install dkms git
```


## clone repository

```bash
git clone https://github.com/shpi/cyc8c2-touchdriver
```

## install


```bash
cd cyc8c2-touchdriver
sudo ln -s /home/pi/cyc8c2-touchdriver /usr/src/cyc8c2-1.0
sudo dkms install cyc8c2/1.0
sudo dtc -I dts -O dtb -o /boot/overlays/touch.dtbo touch.dts
```
