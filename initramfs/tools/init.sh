#!/bin/busybox sh
/bin/busybox --install -s /bin

Rescue_Shell(){
  clear
  echo -e "Something went wrong...\e[?25h" > /dev/tty1
  exec sh
}

mount -t proc none /proc
mount -t sysfs none /sys
mount -t devtmpfs none /dev
mdev -s
echo -e "\e[?25l"
n=0
until [ $n -ge 3 ]
do
   mount -o rw /dev/mmcblk0p1 /mnt/boot &> /dev/null  && break
   let "n=n+1"
   sleep .3
done
[[ $n -ge 3 ]] && echo "FAILED TO MOUNT BOOT"


init_display


# Auslesen der Seriennummer
while read -r line; do
    case "$line" in
        *Serial*)
            serial="${line##*: }"
            break
            ;;
    esac
done < /proc/cpuinfo


hardwarecheck -s


if [[ -f /mnt/boot/splash.txt ]]; then
   SPLASH_IMG=$(awk -F '=' '/^image/{print $2}' /mnt/boot/splash.txt)
   SPLASH_FS=$(awk -F '=' '/^fullscreen/{print $2}' /mnt/boot/splash.txt)
   SPLASH_ASP=$(awk -F '=' '/^stretch/{print $2}' /mnt/boot/splash.txt)
fi

[[ -z $SPLASH_IMG ]] && SPLASH_IMG="splash.png"
if [[ -n $SPLASH_FS ]]; then
   [[ "$SPLASH_FS" = "1" ]] && SPLASH_OPT="--resize" || unset SPLASH_FS
fi
if [[ -n $SPLASH_ASP ]]; then
   [[ "$SPLASH_ASP" = "1" ]] && SPLASH_OPT="--resize --freeaspect" || unset SPLASH_ASP
fi

[[ -f "/mnt/boot/$SPLASH_IMG" ]] && fbsplash $SPLASH_OPT "/mnt/boot/$SPLASH_IMG" || echo "NO SPLASH IMAGE FOUND!!"



found=0
 while read -r line; do
    case "$line" in
        "[0x$serial]")
            found=1
            ;;
        model=*)
            MODEL=${line#model=}
            ;;
    esac
done < "/mnt/boot/config.txt"

echo -ne "\033[0;0H" #jump to top left

    if [ $found -eq 1 ]; then
        echo "Model: $MODEL"
    else
        echo "Model discovery necessary. Starting..."
        model_discovery
    fi


umount /mnt/boot
mount -o ro /dev/mmcblk0p2 /mnt/root || Rescue_Shell
umount /proc /sys
mount --move /dev /mnt/root/dev
exec switch_root /mnt/root /sbin/init