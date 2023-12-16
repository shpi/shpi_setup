#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/usbdevice_fs.h>
#include <linux/usb/ch9.h>
#include <dirent.h>
#include <errno.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h> // For usleep


#define VENDOR_ID  0x03eb
#define PRODUCT_ID 0x2ff4

#define USB_REQ_TYPE_CLASS (0x01 << 5)
#define USB_REQ_RECIPIENT_INTERFACE 0x01
#define USB_ENDPOINT_IN 0x80

#define DFU_GETSTATUS 3
#define DFU_CLRSTATUS 4
#define DFU_DNLOAD 1

#define USB_CTRL_GET_TIMEOUT 5000 // Timeout in milliseconds

int usb_control_msg(int fd, uint8_t request_type, uint8_t request,
                    uint16_t value, uint16_t index, unsigned char *data,
                    uint16_t size, unsigned int timeout) {
    struct usbdevfs_ctrltransfer ctrl;
    memset(&ctrl, 0, sizeof(ctrl));
    ctrl.bRequestType = request_type;
    ctrl.bRequest = request;
    ctrl.wValue = value;
    ctrl.wIndex = index;
    ctrl.wLength = size;
    ctrl.timeout = timeout;
    ctrl.data = data;

    return ioctl(fd, USBDEVFS_CONTROL, &ctrl);
}

int main() {
    DIR *bus_dir, *dev_dir;
    struct dirent *bus_dp, *dev_dp;
    char bus_path[24] = "/dev/bus/usb";
    char dev_path[32];
    int found = 0;

    bus_dir = opendir(bus_path);
    if (!bus_dir) {
        perror("Cannot open /dev/bus/usb");
        return 1;
    }

    while ((bus_dp = readdir(bus_dir)) != NULL && !found) {
        if (bus_dp->d_name[0] == '.') continue;

        sprintf(bus_path, "/dev/bus/usb/%s", bus_dp->d_name);
        dev_dir = opendir(bus_path);
        if (!dev_dir) continue;

        while ((dev_dp = readdir(dev_dir)) != NULL && !found) {
            if (dev_dp->d_name[0] == '.') continue;

            sprintf(dev_path, "%s/%s", bus_path, dev_dp->d_name);
            int fd = open(dev_path, O_RDWR);
            if (fd < 0) continue;

            struct usb_device_descriptor desc;
            int ret = ioctl(fd, USBDEVFS_CONTROL, &(struct usbdevfs_ctrltransfer){
                .bRequestType = USB_DIR_IN | USB_TYPE_STANDARD | USB_RECIP_DEVICE,
                .bRequest = USB_REQ_GET_DESCRIPTOR,
                .wValue = USB_DT_DEVICE << 8,
                .wIndex = 0,
                .wLength = sizeof(desc),
                .data = &desc,
            });
            close(fd);

            if (ret < 0) continue;

            if (desc.idVendor == VENDOR_ID && desc.idProduct == PRODUCT_ID) {
                found = 1;
                break;
            }
        }

        closedir(dev_dir);
    }

    closedir(bus_dir);

    if (!found) {
        printf("Device not found\n");
        return 1;
    }

    // Device found, now open and send commands
    int fd = open(dev_path, O_RDWR);
    if (fd < 0) {
        perror("Failed to open device");
        return 1;
    }

    unsigned char dfu_status[6];
    unsigned char dnload_data[5] = {0x04, 0x03, 0x01, 0x00, 0x00};

    // DFU_GETSTATUS request
    if (usb_control_msg(fd, USB_REQ_TYPE_CLASS | USB_REQ_RECIPIENT_INTERFACE | USB_ENDPOINT_IN,
                        DFU_GETSTATUS, 0, 0, dfu_status, sizeof(dfu_status), USB_CTRL_GET_TIMEOUT) < 0) {
        fprintf(stderr, "Error in DFU_GETSTATUS: %s\n", strerror(errno));
    }

    usleep(100);

    // DFU_CLRSTATUS request
    if (usb_control_msg(fd, USB_REQ_TYPE_CLASS | USB_REQ_RECIPIENT_INTERFACE,
                        DFU_CLRSTATUS, 0, 0, NULL, 0, USB_CTRL_GET_TIMEOUT) < 0) {
        fprintf(stderr, "Error in DFU_CLRSTATUS: %s\n", strerror(errno));
    }

    usleep(100);

    // DFU_DNLOAD request
    if (usb_control_msg(fd, USB_REQ_TYPE_CLASS | USB_REQ_RECIPIENT_INTERFACE,
                        DFU_DNLOAD, 0, 0, dnload_data, sizeof(dnload_data), USB_CTRL_GET_TIMEOUT) < 0) {
        fprintf(stderr, "Error in DFU_DNLOAD: %s\n", strerror(errno));
    }

    usleep(100);


    // DFU_CLRSTATUS request
    if (usb_control_msg(fd, USB_REQ_TYPE_CLASS | USB_REQ_RECIPIENT_INTERFACE,
                        DFU_CLRSTATUS, 0, 0, NULL, 0, USB_CTRL_GET_TIMEOUT) < 0) {
        fprintf(stderr, "Error in DFU_CLRSTATUS: %s\n", strerror(errno));
    }



    close(fd);
    return 0;
}
