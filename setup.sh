#!/bin/sh -e

I2CDETECT="/usr/sbin/i2cdetect"
I2CGET="/usr/sbin/i2cget"
I2CSET="/usr/sbin/i2cset"

SLEEP="/bin/sleep"
PYTHON3="/usr/bin/python3"
DFUP="/usr/bin/dfu-programmer"
LSUSB="/usr/bin/lsusb"
FLASHCC="/home/pi/shpi_setup/flashCC"
GREP="/bin/grep"
UNAME="/bin/uname"

ATMEGA_USB="03eb:2ff4" #> no lite
HUB8836_USB="214b:7000" #> no lite, no one
HUB8836A_USB="214b:7250" #> one without ethernet, maybe future zeros
ZIGBEE_USB="0451:16ae" #> zero or one with zigbee
CP2102_USB="10c4:ea60" #> one
HS100B_USB="0d8c:0014" #> no zero prototype
HUB9514_USB="0424:9514" #> one

I2C_ATMEGA="0x2a" #> no lite
I2C_AHT10="0x38" #> zero & zero lite, no one
I2C_SHT3x="0x44" 
I2C_VCNL4010="0x13" #> switch / one plus
I2C_BH1750="0x23" #> all, error if missing
I2C_INA219="0x45" #> one
I2C_ADS1015="0x48" #> zero lite
I2C_BMP280="0x77" #> multiple
I2C_TOUCH_ONE="0x5d"  #> one
I2C_TOUCH_ONE2="0x14" #> one
I2C_TOUCH_ZERO="0x5c" #> zero / zero lite / switch
I2C_CALIPILE="0x0C" #> one
I2C_MLX="0x5b" #> all

AVR_ZERO_FW="/home/pi/shpi_setup/zero-main.hex"
AVR_ONE_FW="/home/pi/shpi_setup/one-main.hex"
ZIGBEE_FW="/home/pi/shpi_setup/cc2531.hex"

HTMLROOT="/var/www/html"



# CHECKING ATMEGA

{ $I2CGET -y 2 "$I2C_ATMEGA" 2> /dev/null; err="$?"; } || true
case "$err" in
  0) echo -n "ATMEGA on I2C\n"
     I2C_ATMEGA=true;;
  1) echo -n "ATMEGA busy on I2C (usually kernel module loaded)\n"
     I2C_ATMEGA=true;;
  2) echo -n "ATMEGA not on I2C\n"
     I2C_ATMEGA=false;;
  *) echo -n "ATMEGA unknown error\n"
     I2C_ATMEGA=false;;
esac

if [ "$I2C_ATMEGA" = false ] ; then

	if ( $LSUSB | $GREP -q "$ATMEGA_USB" 2> /dev/null ) ; then
		echo "ATMEGA DFU USB detected.\n"
		ATMEGA_USB=true
	else
		echo "No ATMEGA detected. Seems to be a Lite version?\n"
		ATMEGA_USB=false
	fi
fi


# CHECKING AHT10

{ $I2CGET -y 2 "$I2C_AHT10" 2> /dev/null; err="$?"; } || true
case "$err" in
  0) echo -n "AHT10 on I2C\n"
     I2C_AHT10=true;;
  1) echo -n "AHT10 busy on I2C (usually kernel module loaded)\n"
     I2C_AHT10=true;;
  2) echo -n "AHT10 not on I2C\n"
     I2C_AHT10=false;;
  *) echo -n "AHT10 unknown error\n"
     I2C_AHT10=false;;
esac

# CHECKING MLX90615

{ $I2CGET -y 2 "$I2C_MLX" 2> /dev/null; err="$?"; } || true
case "$err" in
  0) echo -n "MLX on I2C\n"
     I2C_MLX=true;;
  1) echo -n "MLX busy on I2C (usually kernel module loaded)\n"
     I2C_MLX=true;;
  2) echo -n "MLX not on I2C\n"
     I2C_MLX=false;;
  *) echo -n "MLX unknown error\n"
     I2C_MLX=false;;
esac

# CHECKING CALIPILE
{ $I2CSET -a -y 2 0x00 0x04 2> /dev/null; } || true
{ $I2CGET -y 2 "$I2C_CALIPILE" 2> /dev/null; err="$?"; } || true
case "$err" in
  0) echo -n "CALIPILE on I2C\n"
     I2C_CALIPILE=true;;
  1) echo -n "CALIPILE busy on I2C (usually kernel module loaded)\n"
     I2C_CALIPILE=true;;
  2) echo -n "CALIPILE not on I2C\n"
     I2C_CALIPILE=false;;
  *) echo -n "CALIPILE unknown error\n"
     I2C_CALIPILE=false;;
esac




# CHECKING VCNL4010

{ $I2CGET -y 2 "$I2C_VCNL4010" 2> /dev/null; err="$?"; } || true
case "$err" in
  0) echo -n "VCNL4010 on I2C\n"
     I2C_VCNL4010=true;;
  1) echo -n "VCNL4010 busy on I2C (usually kernel module loaded)\n"
     I2C_VCNL4010=true;;
  2) echo -n "VCNL4010 not on I2C\n"
     I2C_VCNL4010=false;;
  *) echo -n "VCNL4010 unknown error\n"
     I2C_VCNL4010=false;;
esac


# CHECKING ADS1015

{ $I2CGET -y 2 "$I2C_ADS1015" 2> /dev/null; err="$?"; } || true
case "$err" in
  0) echo -n "ADS1015 on I2C\n"
     I2C_ADS1015=true;;
  1) echo -n "ADS1015 busy on I2C (usually kernel module loaded)\n"
     I2C_ADS1015=true;;
  2) echo -n "ADS1015 not on I2C\n"
     I2C_ADS1015=false;;
  *) echo -n "ADS1015 unknown error\n"
     I2C_ADS1015=false;;
esac



# CHECKING BH1750

{ $I2CGET -y 2 "$I2C_BH1750" 2> /dev/null; err="$?"; } || true
case "$err" in
  0) echo -n "BH1750 on I2C\n"
     I2C_BH1750=true;;
  1) echo -n "BH1750 busy on I2C (usually kernel module loaded)\n"
     I2C_BH1750=true;;
  2) echo -n "BH1750 not on I2C\n"
     I2C_BH1750=false;;
  *) echo -n "BH1750 unknown error\n"
     I2C_BH1750=false;;
esac

# CHECKING INA219

{ $I2CGET -y 2 "$I2C_INA219" 2> /dev/null; err="$?"; } || true
case "$err" in
  0) echo -n "INA219 on I2C\n"
     I2C_INA219=true;;
  1) echo -n "INA219 busy on I2C (usually kernel module loaded)\n"
     I2C_INA219=true;;
  2) echo -n "INA219 not on I2C\n"
     I2C_INA219=false;;
  *) echo -n "INA219 unknown error\n"
     I2C_INA219=false;;
esac



# CHECKING BMP280

{ $I2CGET -y 2 "$I2C_BMP280" 2> /dev/null; err="$?"; } || true
case "$err" in
  0) echo -n "BMP280 on I2C\n"
     I2C_BMP280=true;;
  1) echo -n "BMP280 busy on I2C (usually kernel module loaded)\n"
     I2C_BMP280=true;;
  2) echo -n "BMP280 not on I2C\n"
     I2C_BMP280=false;;
  *) echo -n "BMP280 unknown error\n"
     I2C_BMP280=false;;
esac




# CHECKING SHT30

{ $I2CGET -y 2 "$I2C_SHT3x" 2> /dev/null; err="$?"; } || true
case "$err" in
  0) echo -n "SHT3x on I2C\n"
     I2C_SHT3x=true;;
  1) echo -n "SHT3x busy on I2C (usually kernel module loaded)\n"
     I2C_SHT3x=true;;
  2) echo -n "SHT3x not on I2C\n"
     I2C_SHT3x=false;;
  *) echo -n "SHT3x unknown error\n"
     I2C_SHT3x=false;;
esac

# CHECKING SHPI ONE TOUCH

{ $I2CGET -y 2 "$I2C_TOUCH_ONE" 2> /dev/null; err="$?"; } || true
case "$err" in
  0) echo -n "Touchscreen SHPI.one  on I2C\n"
     I2C_TOUCH_ONE=true;;
  1) echo -n "Touchscreen SHPI.one busy on I2C (usually kernel module loaded)\n"
     I2C_TOUCH_ONE=true;;
  2) I2C_TOUCH_ONE=false;;
  *) I2C_TOUCH_ONE=false;;
esac

if [ "$I2C_TOUCH_ONE" = false ] ; then


{ $I2CGET -y 2 "$I2C_TOUCH_ONE2" 2> /dev/null; err="$?"; } || true
case "$err" in
  0) echo -n "Touchscreen SHPI.one  on I2C\n"
     I2C_TOUCH_ONE=true;;
  1) echo -n "Touchscreen SHPI.one busy on I2C (usually kernel module loaded)\n"
     I2C_TOUCH_ONE=true;;
  2) echo -n "Touchscreen SHPI.one not on I2C\n"
     I2C_TOUCH_ONE=false;;
  *) echo -n "Touchscreen SHPI.one unknown error\n"
     I2C_TOUCH_ONE=false;;
esac

fi



{ $I2CGET -y 2 "$I2C_TOUCH_ZERO" 2> /dev/null; err="$?"; } || true
case "$err" in
  0) echo -n "Touchscreen A035VW01  on I2C\n"
     I2C_TOUCH_ZERO=true;;
  1) echo -n "Touchscreen A035VW01 busy on I2C (usually kernel module loaded)\n"
     I2C_TOUCH_ZERO=true;;
  2) echo -n "Touchscreen A035VW01 not on I2C\n"
     I2C_TOUCH_ZERO=false;;
  *) echo -n "Touchscreen A035VW01 unknown error\n"
     I2C_TOUCH_ZERO=false;;
esac



if ( $LSUSB | $GREP -q "$HS100B_USB" 2> /dev/null ) ; then
                echo "CMEDIA HS100B SOUND USB detected.\n"
                HS100B_USB=true
else
                echo "No CMEDIA HS100B detected. No Sound available!\n"
                HS100B_USB=false
fi

if ( $LSUSB | $GREP -q "$ZIGBEE_USB" 2> /dev/null ) ; then
                echo "TI ZIGBEE USB CHIP detected.\n"
                ZIGBEE_USB=true
else
                echo "No TI ZIGBEE CC2531 detected.\n"
                ZIGBEE_USB=false
fi



if ( $LSUSB | $GREP -q "$HUB9514_USB" 2> /dev/null ) ; then
                echo "SMSC9514 HUB with Ethernet detected.\n"
                HUB9514_USB=true
else
                echo "No SMSC9514 HUB with Ethernet detected.\n"
                HUB9514_USB=false
fi



if ( $LSUSB | $GREP -q "$HUB8836_USB" 2> /dev/null ) ; then
                echo "8836 USB HUB detected.\n"
                HUB8836_USB=true
else
                echo "No 8836 HUB  detected.\n"
                HUB8836_USB=false
fi

if ( $LSUSB | $GREP -q "$HUB8836A_USB" 2> /dev/null ) ; then
                echo "8836A USB HUB detected.\n"
                HUB8836A_USB=true
else
                echo "No 8836A HUB  detected.\n"
                HUB8836A_USB=false
fi




# CHECKING FOR HARDWARE VERSION

if ( $UNAME -m | $GREP -q "armv6l" 2> /dev/null ) ; then

echo "We try to determine your hardware version, to setup your device."

# SHPI.one

if   { [ "$I2C_ATMEGA" = true ]   || [ "$ATMEGA_USB" = true   ]; } &&
     { [ "$I2C_MLX" = true ]      || [ "$I2C_CALIPILE" = true ]; } &&
     { [ "$HUB8836A_USB" = true ] || [ "$HUB9514_USB" = true  ]; } &&
     [ "$I2C_SHT3x" = true ]   				   &&
     [ "$I2C_TOUCH_ZERO" = false ]   			   &&
     [ "$I2C_TOUCH_ONE" = true ]			   &&
     [ "$I2C_ADS1015" = false ]     			   &&
     [ "$I2C_BMP280" = true ]       			   &&
     [ "$I2C_BH1750" = true ]     			   &&
     [ "$HUB8836_USB" = false ]    			   &&
     [ "$HS100B_USB" = true ]  ; then

        echo "MODEL DETECTED: SHPI.one detected.";

fi



# SHPI.zero prototype

if   [ "$I2C_TOUCH_ZERO" = true ]  &&
     [ "$I2C_ADS1015" = false ]    &&
     [ "$I2C_BMP280" = true ]      &&
     [ "$I2C_BH1750" = true ]      &&
     [ "$I2C_MLX" = true ]         &&
     [ "$HUB8836_USB" = false ]    &&
     [ "$HUB8836A_USB" = false ]   &&
     [ "$HUB9514_USB" = false ]    &&
     [ "$HS100B_USB" = false ]     &&
     [ "$I2C_SHT3x" = true ]	  &&
     { [ "$I2C_ATMEGA" = true ] || "$ATMEGA_USB" = true ] ; } ; then

        echo "MODEL DETECTED: SHPI.zero prototype";





# SHPI.zero

elif   [ "$I2C_AHT10" = true ]       &&
     [ "$I2C_TOUCH_ZERO" = true ]  &&
     [ "$I2C_ADS1015" = false ]    &&
     [ "$I2C_BMP280" = true ]      &&
     [ "$I2C_BH1750" = true ]      &&
     [ "$I2C_MLX" = true ]         &&
     { [ "$I2C_ATMEGA" = true ]   || [ "$ATMEGA_USB" = true ] ; }  &&
     { [ "$HUB8836_USB" = true ]  || [ "$HUB8836A_USB" = true ] ; }  &&
     [ "$HUB9514_USB" = false ]  &&
     [ "$HS100B_USB" = true ]  ; then

        echo "MODEL DETECTED: SHPI.zero";




# SHPI.zero lite prototype

elif   [ "$I2C_ATMEGA" = false ]     &&
     [ "$ATMEGA_USB" = false ]     &&
     [ "$I2C_SHT3x" = true ]       &&
     [ "$I2C_TOUCH_ZERO" = true ]  &&
     [ "$I2C_ADS1015" = true ]     &&
     [ "$I2C_BMP280" = true ]      &&
     [ "$I2C_BH1750" = true ]      &&
     [ "$I2C_MLX" = true ]         &&
     [ "$HUB8836_USB" = false ]    &&
     [ "$HUB8836A_USB" = false ]   &&
     [ "$HUB9514_USB" = false ]   &&
     [ "$HS100B_USB" = true ]  ; then

	echo "MODEL DETECTED: SHPI.zero lite prototype";




# SHPI.zero lite

elif   [ "$I2C_ATMEGA" = false ]     &&
     [ "$ATMEGA_USB" = false ]     &&
     [ "$I2C_AHT10" = true ]       &&
     [ "$I2C_TOUCH_ZERO" = true ]  &&
     [ "$I2C_BH1750" = true ]      &&
     [ "$I2C_MLX" = true ]         &&
     [ "$HUB8836_USB" = false ]    &&
     [ "$HUB8836A_USB" = false ]   &&
     [ "$HUB9514_USB" = false ]   &&
     [ "$HS100B_USB" = true ] ; then

        echo "MODEL DETECTED: SHPI.zero lite";

fi

else echo "No armv6l detected. Stopping."

fi


