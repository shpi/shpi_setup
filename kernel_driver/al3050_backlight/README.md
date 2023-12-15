# AL3050 Backlight driver
AL3050 kernel backlight driver


## prerequisites

```bash
sudo apt-get install dkms git
```


## clone repository

```bash
git clone https://github.com/shpi/al3050_backlight
```

## install


```bash
cd al3050_backlight
sudo ln -s $HOME/al3050_backlight /usr/src/al3050_backlight-1.0
sudo dkms install al3050_backlight/1.0
sudo dtc -I dts -O dtb -o /boot/overlays/touch.dtbo touch.dts
```
