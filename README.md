## Please note:

This is in rework. We provide here als files to setup an custom SHPI image on your own. Normal users should stay away and use our image.



## enable SSH:

Enable SSH by placing a file named ssh (without extension), onto the boot partition of the SD card. The content of the file does not matter.

Raspbian will have two partitions. Place the ssh file on to the smaller boot partition.



## setup WIFI:

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






