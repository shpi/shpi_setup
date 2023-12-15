# mlx90615-iio
MLX90615 kernel iio driver, i2c address 0x5b

guide for raspberry pi 

## prerequisites
```bash
sudo apt-get install dkms git
```

## clone repository

```bash
git clone https://github.com/shpi/mlx90615_iio
```

## install
```bash
cd mlx90615_iio
   sudo ln -s /home/pi/mlx90615_iio /usr/src/mlx90615-1.0
   sudo dkms install mlx90615/1.0 
   sudo dtc -I dts -O dtb -o /boot/overlays/mlx90615.dtbo mlx90615.dts
   ```
   

## /boot/config.txt

add

```bash
dtoverlay=mlx90615
```

## one shot access

```bash
cat /sys/bus/iio/devices/iio\:device0/in_temp_object_raw 
cat /sys/bus/iio/devices/iio\:device0/in_temp_ambient_raw
cat /sys/bus/iio/devices/iio\:device0/in_temp_offset
cat /sys/bus/iio/devices/iio\:device0/in_temp_scale
```
Calculate temperature (millicelsius): (in_temp_object_raw + in_temp_offset) * in_temp_scale
