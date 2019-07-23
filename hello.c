#include <bcm2835.h>
#include <stdio.h>
#include <inttypes.h>

#define BCM2835_PAD_SLEW_RATE      (0x00 << 2)

char hexval[5];

int main(int argc, char **argv)
{
    if (!bcm2835_init())
      return 1;

    //bcm2835_gpio_set_pad(BCM2835_PAD_GROUP_GPIO_0_27, 
    bcm2835_gpio_set_pad(BCM2835_PAD_GROUP_GPIO_0_27, BCM2835_PAD_DRIVE_2mA);
 bcm2835_gpio_set_pad(BCM2835_PAD_GROUP_GPIO_46_53, BCM2835_PAD_DRIVE_2mA);
 bcm2835_gpio_set_pad(BCM2835_PAD_GROUP_GPIO_28_45, BCM2835_PAD_SLEW_RATE);

 bcm2835_gpio_set_pad(BCM2835_PAD_GROUP_GPIO_46_53, BCM2835_PAD_SLEW_RATE);

 bcm2835_gpio_set_pad(BCM2835_PAD_GROUP_GPIO_28_45, BCM2835_PAD_DRIVE_2mA);


    bcm2835_gpio_set_pad(BCM2835_PAD_GROUP_GPIO_0_27, BCM2835_PAD_SLEW_RATE);


    printf(hexval);
    bcm2835_close();
    return 0;
}
