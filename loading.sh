#!/bin/sh -e

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
