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
