# SHPI Zero Lite Backlight driver

PWM based driver.



## prerequisites

```bash
sudo apt-get install dkms git
```


## clone repository

```bash
git clone https://github.com/shpi/lite_backlight
```

## install


```bash
cd lite-backlight
sudo ln -s $HOME/lite_backlight /usr/src/lite_backlight-1.0
sudo dkms install lite_backlight/1.0
```
