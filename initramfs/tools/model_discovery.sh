#!/bin/busybox sh

# Auslesen der Seriennummer
while read -r line; do
    case "$line" in
        *Serial*)
            serial="${line##*: }"
            break
            ;;
    esac
done < /proc/cpuinfo

echo "Serial:$serial" > /mnt/boot/log.txt

hardwarecheck | tee -a /mnt/boot/log.txt

I2C_DEVICES="
ATMEGA=2a
AHT10=38
SHT3x=44
VCNL4010=13
BH1750=23
INA219=45
ADS1015=48
BMP280=77
TOUCH_ONE=5d
TOUCH_ONE2=14
TOUCH_ZERO=5c
CALIPILE=0C
MLX=5b
"

for entry in $I2C_DEVICES; do
    NAME="${entry%%=*}"
    ADDRESS="${entry##*=}"

    # Capture the output and exit status of i2cget
    i2cget_output=$(i2cget -s"$ADDRESS" -dr -ib 1)
    i2cget_status=$?

    # Echo the name and i2cget output to the log file
    echo -n "$NAME " | tee -a /mnt/boot/log.txt
    echo "$i2cget_output" | tee -a /mnt/boot/log.txt

    # Set the variable to 1 if i2cget was successful, else to 0
    if [ "$i2cget_status" -eq 0 ]; then
        eval "$NAME=1"
    else
        eval "$NAME=0"
    fi
done

# Define USB devices and their IDs
USB_DEVICES="
ATMEGA_USB=03eb:2ff4
HUB8836_USB=214b:7000
HUB8836A_USB=214b:7250
ZIGBEE_USB=0451:16ae
CP2102_USB=10c4:ea60
HS100B_USB=0d8c:0014
HUB9514_USB=0424:9514
"

usb_devices_path="/sys/bus/usb/devices/"


for entry in $USB_DEVICES; do
    NAME="${entry%%=*}"
    ID="${entry##*=}"
    eval "$NAME=0"  # Initialize the device variable to 0 (not found)

    for device in ${usb_devices_path}*; do
        if [ -f "${device}/idVendor" ] && [ -f "${device}/idProduct" ]; then
            idVendor=$(cat "${device}/idVendor")
            idProduct=$(cat "${device}/idProduct")
            deviceID="${idVendor}:${idProduct}"

            if [ "$deviceID" = "$ID" ]; then
                eval "$NAME=1"  # Set the device variable to 1 (found)
                echo "$NAME=1" | tee -a /mnt/boot/log.txt
                break
            fi
        fi
    done
done



for entry in $USB_DEVICES; do
    NAME="${entry%%=*}"
    eval "status=\$$NAME"  # Retrieve the status
    echo "$NAME: $status"
done


MODEL_NAMES="shpi_one shpi_zero_prototype shpi_zero shpi_zero_lite_prototype shpi_zero_lite"

shpi_one="
ATMEGA|ATMEGA_USB=1
MLX|CALIPILE=1
HUB8836A_USB|HUB9514_USB=1
SHT3x=1
TOUCH_ONE|TOUCH_ONE2=1
ADS1015=0
INA219=1
HUB8836_USB=0
HS100B_USB=1
"

shpi_zero_prototype="
TOUCH_ZERO=1
ADS1015=0
BMP280=1
HUB8836_USB=0
HUB8836A_USB=0
HUB9514_USB=0
HS100B_USB=0
SHT3x=1
ATMEGA|ATMEGA_USB=1
"

shpi_zero="
AHT10=1
TOUCH_ZERO=1
ADS1015=0
BMP280=1
ATMEGA|ATMEGA_USB=1
HUB8836_USB|HUB8836A_USB=1
HUB9514_USB=0
HS100B_USB=1
"

shpi_zero_lite_prototype="
ATMEGA=0
ATMEGA_USB=0
SHT3x=1
TOUCH_ZERO=1
TOUCH_ONE=0
ADS1015=1
BMP280=1
BH1750=1
HUB8836_USB=0
HUB8836A_USB=0
HUB9514_USB=0
HS100B_USB=1
"
shpi_zero_lite="
AHT10=1
BH1750=1
TOUCH_ZERO=1
MLX=1
HS100B_USB=1
ATMEGA=0
ATMEGA_USB=0
ADS1015=0
CALIPILE=0
HUB9514_USB=0
HUB8836A_USB=0
HUB8836_USB=0
TOUCH_ONE=0
TOUCH_ONE2=0
"

for model in $MODEL_NAMES; do
    # Dynamically get the conditions for the current model
    eval "conditions=\$$model"

    conditions_met=true
    for condition in $conditions; do
        devices="${condition%%=*}"
        expected_status="${condition##*=}"
        
        case "$devices" in
            *|*)
                or_condition_met=false
                oldIFS="$IFS"
                IFS='|'
                set -f # disable globbing
                for device in $devices; do
                    eval "actual_status=\$$device"
                    if [ "$actual_status" = "$expected_status" ]; then
                        or_condition_met=true
                        break
                    fi
                done
                IFS="$oldIFS"
                set +f # enable globbing
                if [ "$or_condition_met" = false ]; then
                    conditions_met=false
                    break
                fi
                ;;
            *)
                eval "actual_status=\$$devices"
                if [ "$actual_status" != "$expected_status" ]; then
                    conditions_met=false
                    break
                fi
                ;;
        esac
    done

    if [ "$conditions_met" = true ]; then
        echo "$model model conditions are met."
        # Perform actions for the model
        echo "[0x$serial]" >> /mnt/boot/config.txt
        echo "model=$model" >> /mnt/boot/config.txt
        echo "dtoverlay=${model}" >> /mnt/boot/config.txt

        case "$model" in
        shpi_zero*)
        echo "dpi_output_format=0x06F216" >> /mnt/boot/config.txt
        echo "display_rotate=2" >> /mnt/boot/config.txt
        echo "hdmi_timings=800 0 50 20 70 480 0 3 1 3 0 0 0 60 0 32000000 6" >> /mnt/boot/config.txt
        ;;
        shpi_one*)
        echo "dpi_output_format=0x07F216" >> /mnt/boot/config.txt
        echo "display_rotate=3" >> /mnt/boot/config.txt
        echo "hdmi_timings=480 0 10 16 59 800 0 15 113 15 0 0 0 60 0 32000000 6" >> /mnt/boot/config.txt
        ;;
        esac


        sync
        umount /mnt/boot
        echo b > /proc/sysrq-trigger
        break
    fi
done

if [ "$conditions_met" != true ]; then
    echo "No SHPI model conditions met. Please check for further instructions." | tee -a /mnt/boot/log.txt
    sleep 5
fi









