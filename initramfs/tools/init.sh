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


# Auslesen der Seriennummer
while read -r line; do
    case "$line" in
        *Serial*)
            serial="${line##*: }"
            break
            ;;
    esac
done < /proc/cpuinfo


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

#make usb error port3 inivisble for ONE, due to pullups resistors for zm5304
printf "\e[30m"



if [ $found -eq 1 ]; then
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




case "$MODEL" in
    shpi_zero_lite*)
        init_display
        ;;
    *)
        init_atmega 
        ;;
esac


    MODEL=${MODEL#shpi}
    # Remove underscores and convert to uppercase manually
    NEW_MODEL=""
    MODEL_LENGTH=${#MODEL}
    i=0
    while [ $i -lt $MODEL_LENGTH ]; do
        CHAR="${MODEL:$i:1}"
        if [ "$CHAR" = "_" ]; then
            CHAR=" "  # Replace underscore with space
        else
            # Manual uppercase conversion using case statement
            case "$CHAR" in
                [a-z]) CHAR=$(printf "\\$(printf '%03o' $(( $(printf '%d' "'$CHAR") - 32 )))") ;;
            esac
        fi
        NEW_MODEL="${NEW_MODEL}${CHAR}"
        i=$((i + 1))
    done
    WHITE='\033[1;37m'
    NO_COLOR='\033[30m'
    echo -e "\033[18;60H${WHITE}$NEW_MODEL${NO_COLOR}"
else
    echo "Model discovery necessary. Starting..."
    model_discovery
fi

umount /mnt/boot
mount -o ro /dev/mmcblk0p2 /mnt/root || Rescue_Shell
umount /proc /sys
mount --move /dev /mnt/root/dev
exec switch_root /mnt/root /sbin/init
