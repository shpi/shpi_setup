#!/bin/sh -e

# Define ATmega device ID
ATMEGA_ID="03eb:2ff4"
ATMEGA_FOUND=0
MODEL=""

# Read the model from /boot/config.txt
while read -r line; do
    case "$line" in
        model=*)
            MODEL=${line#model=}
            ;;
    esac
done < "/boot/config.txt"


# Proceed only if a model is found
if [ -n "$MODEL" ]; then
    # Check if ATMEGA is connected
    for device in /sys/bus/usb/devices/*; do
        if [ -f "${device}/idVendor" ] && [ -f "${device}/idProduct" ]; then
            idVendor=$(cat "${device}/idVendor")
            idProduct=$(cat "${device}/idProduct")
            if [ "${idVendor}:${idProduct}" = "$ATMEGA_ID" ]; then
                ATMEGA_FOUND=1
                break
            fi
        fi
    done

    # Proceed with flashing only if ATMEGA is found
    if [ $ATMEGA_FOUND -eq 1 ]; then
        # Determine which .hex file to flash based on the model
        case "$MODEL" in
            *one*)
                HEX_FILE="one.hex"
                ;;
            *zero*)
                HEX_FILE="zero.hex"
                ;;
        esac

        if [ -n "$HEX_FILE" ]; then
            echo "Flashing $HEX_FILE to ATMEGA device..."
            dfu-programmer atmega32u4 erase
            dfu-programmer atmega32u4 flash "/home/pi/shpi_setup/$HEX_FILE"
            dfu-programmer atmega32u4 start
        fi
    fi
fi

sleep 5

# Calculate the starting column to center the dots (assuming 100 characters per line)
num_dots=60
dots_per_row=30
screen_width_chars=100
start_col=$(( (screen_width_chars - dots_per_row) / 2 ))

# Function to print dots in the center
print_dots() {
    num=$1
    row=$2
    printf "\033[%d;%dH" $row $start_col
    for i in $(seq 1 $num); do
        printf "."
        if [ $i -eq $dots_per_row ]; then
            # Move to the next row
            printf "\n\033[%d;%dH" $((row + 1)) $start_col
        fi
    done
}

# Clear screen and set cursor visibility to false
clear
echo -ne "\033[?25l"

# Print dots with a delay
for i in $(seq 1 $num_dots); do
    print_dots $i 24 # 24 is roughly the vertical center for 30 rows (480px / 16px per row)
    sleep 1
done

# Reset cursor visibility
echo -ne "\033[?25h"
