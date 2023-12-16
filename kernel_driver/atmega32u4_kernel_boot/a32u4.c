#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/usb.h>
#include <linux/firmware.h>
#include <linux/delay.h>


#define a32u4_VENDOR_ID 		0x03eb
#define a32u4_PRODUCT_ID		0x2ff4

#define DFU_DNLOAD      1
#define DFU_GETSTATUS	3
#define DFU_CLRSTATUS	4

static int a32u4_probe(struct usb_interface *intf, const struct usb_device_id *id);
static void a32u4_disconnect(struct usb_interface *intf);


static const struct usb_device_id id_table[] = {
	{ USB_DEVICE(a32u4_VENDOR_ID, a32u4_PRODUCT_ID) },
	{ }                                             /* Terminating entry */
};

MODULE_DEVICE_TABLE (usb, id_table);

struct a32u4_dev {
	struct usb_device *udev;
	unsigned int speed;
};

struct dfu_status {
	unsigned char status;
	unsigned char poll_timeout[3];
	unsigned char state;
	unsigned char string;
} __packed;



static void a32u4_disconnect(struct usb_interface *interface)
{
	struct a32u4_dev *dev;

	dev = usb_get_intfdata (interface);
	usb_set_intfdata(interface, NULL);
	usb_put_dev(dev->udev);
	kfree(dev);
}


static int a32u4_probe(struct usb_interface *intf, const struct usb_device_id *id)
{
        struct usb_device *udev = interface_to_usbdev(intf);
        struct a32u4_dev *dev;
        struct dfu_status *dfu_stat_buf = NULL;

        unsigned char data[5] = { 0x04, 0x03, 0x01, 0x00, 0x00 };
        unsigned char *buf;
        int ret;


        buf = kmalloc(5, GFP_KERNEL);
        memcpy(buf, data, 5);

        dfu_stat_buf = kmalloc(sizeof(struct dfu_status), GFP_KERNEL);
	if (!dfu_stat_buf) {
		ret = -ENOMEM;
		goto error;
	}

        dev = kzalloc(sizeof(struct a32u4_dev), GFP_KERNEL);
	if (!dev) {
		ret = -ENOMEM;
		goto error;
	}

	dev->udev = usb_get_dev(udev);
        dev_info(&udev->dev,"Starting ATmega32u4 in 1s...\n");
        msleep(1000); // Sleep for 1000 milliseconds or 1 second



	usb_set_intfdata(intf, dev);


       ret = usb_control_msg(dev->udev, usb_rcvctrlpipe(dev->udev, 0), DFU_GETSTATUS,
			      USB_TYPE_CLASS | USB_DIR_IN | USB_RECIP_INTERFACE,
			      0, 0, dfu_stat_buf, sizeof(struct dfu_status),
			      USB_CTRL_GET_TIMEOUT);

       if (ret < 0) {
                        dev_err(&udev->dev,
                                "GET Status failed with %d\n", ret);

                }


       ret = usb_control_msg(dev->udev,
                              usb_sndctrlpipe(dev->udev, 0),
                              DFU_CLRSTATUS,
                              0b00100001,
                              //USB_TYPE_CLASS | USB_DIR_OUT | USB_RECIP_INTERFACE,
                              0,
                              0,
                              NULL,
                              0,
                              USB_CTRL_GET_TIMEOUT);



       if (ret < 0) {
                        dev_err(&udev->dev,
                                "clear status failed %d\n", ret);
                }




       ret = usb_control_msg(dev->udev,
                              usb_sndctrlpipe(dev->udev, 0),
                              DFU_DNLOAD,
                              0b00100001,
                              //USB_TYPE_CLASS | USB_DIR_OUT | USB_RECIP_INTERFACE,
                              0,
                              0,
                              buf,
                              sizeof(data),
                              USB_CTRL_GET_TIMEOUT);



       if (ret < 0) {
                        dev_err(&udev->dev,
                                "1. start command failed with %d\n", ret);
                }

   

        ret = usb_control_msg(dev->udev,
                              usb_sndctrlpipe(dev->udev, 0),
                              DFU_DNLOAD,
                              0b00100001,
                              //USB_TYPE_CLASS | USB_DIR_OUT | USB_RECIP_INTERFACE,
                              0,
                              0,
                              NULL,
                              0,
                              USB_CTRL_GET_TIMEOUT);



       if (ret < 0) {
                        dev_err(&udev->dev,
                                "2. start command failed with %d\n", ret);
                }


        

	dev_dbg(&intf->dev, "a32u4_probe\n");
	dev_info(&intf->dev, "%s start\n", __func__);


	return 0;


error:
	kfree(dev);
	return ret;

}


static struct usb_driver a32u4_driver = {
	.name		= "a32u4 - fast start",
	.probe		= a32u4_probe,
	.disconnect	= a32u4_disconnect,
	.id_table	= id_table,


};

module_usb_driver(a32u4_driver);

MODULE_AUTHOR("Lutz Harder");
MODULE_DESCRIPTION("ATmega32u4 fast start.");
MODULE_LICENSE("GPL");

