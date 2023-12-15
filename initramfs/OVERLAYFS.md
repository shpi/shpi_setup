# Overlayfs with Raspberry Pi OS

## Introduction

There is an option in ```raspi-config``` under **Advanced Options** for overlayfs this is designed to make your SD card read-only very simple.  The problem is this also uses an initramfs and makes it incompatible with this project out of the box!

## How to work-around

It's possible to use a splash image with the overlayfs initramfs this goes back to my original method to generate an initramfs using the **initramfs-update** tools included with Raspberry Pi OS. *please note this is a workaround and isn't supported officially*

***FOLLOW THESE STEPS BEFORE ACTIVATING THE OVERLAYFS***

From the initramfs-splash folder execute the following commands
```
sudo mkdir /var/initramfs-splash
cd /var/initramfs-splash
zcat {PATH TO initramfs-splash}/boot/initramfs.img | sudo cpio -idmv
```
This will unpack the initramfs I've included in this project.  Replace the {PATH TO initramfs-splash} with the correct path where you cloned the repo.  

create ```/etc/initramfs-tools/hooks/splash.sh```, copy in these contents and set the file to be executable.
```
#!/bin/sh
PREREQ=""
prereqs() {
         echo "$PREREQ"
}
case $1 in 
# get pre-requisites 
prereqs)
         prereqs
         exit 0
         ;; esac
# fbcp 

echo "Copy Image"
. /usr/share/initramfs-tools/hook-functions
rm -f ${DESTDIR}/etc/splash.png
cp /boot/splash.png ${DESTDIR}/etc
chmod 666 ${DESTDIR}/etc/splash.png
echo "Copy fbsplash"
cp /var/initramfs-splash/bin/fbsplash ${DESTDIR}/bin
exit 0
```

Next you need to create ```/etc/initramfs-tools/scripts/init-top/splash```, copy these contents into it and set it to executable.
```
#!/bin/sh
fbsplash  /etc/splash.png
```
Edit ```/boot/cmdline.txt``` find the ```console=tty1``` change that to ```console=tty2``` and add the end of the line ```logo.nologo loglevel=0 splash silent quiet```  ***IMPORTANT*** This must be all on one line.

Copy your Splash image to ```/boot/splash.png```

Now you can use the raspi-config to activate the overlayfs

## Limitations

1. The image is *baked* into the initramfs.
2. The configuration file splash.txt isn't used.
3. The generated initramfs is larger than the what I provide and will take longer to load this is necessary for the overlayfs to work.
4. There is no official support or guaranty this will work properly.

## Acknowledgements 
@PiGraham from RaspberryPi Forums brought this to my attention and I'd like to thank them for doing so and I hope this will help if someone needs both these projects.

