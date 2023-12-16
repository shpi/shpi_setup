#!/bin/busybox sh

#/bin/busybox --install -s /bin


MLX=0
CALIPILE=1
ATMEGA=0
ATMEGA_USB=1
ATMEGA_LUFA_USB=1
HUB9514_USB=1
HUB8836A_USB=0
SHT3x=1
TOUCH_ONE=1 
TOUCH_ONE2=1
ADS1015=0
INA219=0
HUB8836_USB=0
HS100B_USB=1


MODEL_NAMES="shpi_one"

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

shpi_one="
ATMEGA|ATMEGA_USB|ATMEGA_LUFA_USB=1
MLX|CALIPILE=1
HUB8836A_USB|HUB9514_USB=1
SHT3x=1
TOUCH_ONE|TOUCH_ONE2=1
ADS1015=0
INA219=1
HUB8836_USB=0
HS100B_USB=1
"


split_string() {
    string=$1
    delimiter=$2
    result=""
    while [ "$string" != "" ]; do
        token=${string%%$delimiter*}
        result="$result $token"
        if [ "$string" = "$token" ]; then
            string=''
        else
            string=${string#*$delimiter}
        fi
    done
    echo $result
}




# Function to check conditions
check_conditions() {
    conditions="$1"
    conditions_met=true
    for condition in $conditions; do
        devices="${condition%%=*}"
        expected_status="${condition##*=}"

        # Check if the condition has an OR ('|')
        if echo "$devices" | grep -q '\|'; then
            # Handle OR conditions
            or_condition_met=false
            for device in $(split_string "$devices" "|"); do
                eval "status=\$$device"
                if [ "$status" = "$expected_status" ]; then
                    or_condition_met=true
                    break
                fi
            done
            if [ "$or_condition_met" = false ]; then
                conditions_met=false
                echo "Condition NOT met for one of: $devices"
            fi
        else
            # Handle single device condition
            eval "status=\$$devices"
            if [ "$status" != "$expected_status" ]; then
                conditions_met=false
                echo "Condition NOT met: $devices"
            fi
        fi
    done

    if [ "$conditions_met" = false ]; then
        echo "One or more conditions not met for model $model."
    else
        echo "All conditions met for model $model."
    fi
}

# Model names and conditions
MODEL_NAMES="shpi_one"
# ... Define conditions for each model

for model in $MODEL_NAMES; do
    # Dynamically get the conditions for the current model
    eval "model_conditions=\$$model"
    
    # Check conditions for the current model
    check_conditions "$model_conditions"
done
