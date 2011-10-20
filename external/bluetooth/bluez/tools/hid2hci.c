/*
 *
 *  BlueZ - Bluetooth protocol stack for Linux
 *
 *  Copyright (C) 2003-2010  Marcel Holtmann <marcel@holtmann.org>
 *
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <string.h>
#include <getopt.h>
#include <sys/ioctl.h>

#include <usb.h>

#ifdef NEED_USB_GET_BUSSES
static inline struct usb_bus *usb_get_busses(void)
{
	return usb_busses;
}
#endif

#ifndef USB_DIR_OUT
#define USB_DIR_OUT	0x00
#endif

static char devpath[PATH_MAX + 1] = "/dev";

struct hiddev_devinfo {
	unsigned int bustype;
	unsigned int busnum;
	unsigned int devnum;
	unsigned int ifnum;
	short vendor;
	short product;
	short version;
	unsigned num_applications;
};

struct hiddev_report_info {
	unsigned report_type;
	unsigned report_id;
	unsigned num_fields;
};

typedef __signed__ int __s32;

struct hiddev_usage_ref {
	unsigned report_type;
	unsigned report_id;
	unsigned field_index;
	unsigned usage_index;
	unsigned usage_code;
	__s32 value;
};

#define HIDIOCGDEVINFO		_IOR('H', 0x03, struct hiddev_devinfo)
#define HIDIOCINITREPORT	_IO('H', 0x05)
#define HIDIOCSREPORT		_IOW('H', 0x08, struct hiddev_report_info)
#define HIDIOCSUSAGE		_IOW('H', 0x0C, struct hiddev_usage_ref)

#define HID_REPORT_TYPE_OUTPUT	2

#define HCI 0
#define HID 1

struct device_info {
	struct usb_device *dev;
	int mode;
	uint16_t vendor;
	uint16_t product;
};

static int switch_csr(struct device_info *devinfo)
{
	struct usb_dev_handle *udev;
	int err;

	udev = usb_open(devinfo->dev);
	if (!udev)
		return -errno;

	err = usb_control_msg(udev, USB_DIR_OUT | USB_TYPE_VENDOR | USB_RECIP_DEVICE,
				0, devinfo->mode, 0, NULL, 0, 10000);

	if (err == 0) {
		err = -1;
		errno = EALREADY;
	} else {
		if (errno == ETIMEDOUT)
			err = 0;
	}

	usb_close(udev);

	return err;
}

static int send_report(int fd, const char *buf, size_t size)
{
	struct hiddev_report_info rinfo;
	struct hiddev_usage_ref uref;
	unsigned int i;
	int err;

	for (i = 0; i < size; i++) {
		memset(&uref, 0, sizeof(uref));
		uref.report_type = HID_REPORT_TYPE_OUTPUT;
		uref.report_id   = 0x10;
		uref.field_index = 0;
		uref.usage_index = i;
		uref.usage_code  = 0xff000001;
		uref.value       = buf[i] & 0x000000ff;
		err = ioctl(fd, HIDIOCSUSAGE, &uref);
		if (err < 0)
			return err;
	}

	memset(&rinfo, 0, sizeof(rinfo));
	rinfo.report_type = HID_REPORT_TYPE_OUTPUT;
	rinfo.report_id   = 0x10;
	rinfo.num_fields  = 1;
	err = ioctl(fd, HIDIOCSREPORT, &rinfo);

	return err;
}

static int switch_logitech(struct device_info *devinfo)
{
	char devname[PATH_MAX + 1];
	int i, fd, err = -1;

	for (i = 0; i < 16; i++) {
		struct hiddev_devinfo dinfo;
		char rep1[] = { 0xff, 0x80, 0x80, 0x01, 0x00, 0x00 };
		char rep2[] = { 0xff, 0x80, 0x00, 0x00, 0x30, 0x00 };
		char rep3[] = { 0xff, 0x81, 0x80, 0x00, 0x00, 0x00 };

		sprintf(devname, "%s/hiddev%d", devpath, i);
		fd = open(devname, O_RDWR);
		if (fd < 0) {
			sprintf(devname, "%s/usb/hiddev%d", devpath, i);
			fd = open(devname, O_RDWR);
			if (fd < 0) {
				sprintf(devname, "%s/usb/hid/hiddev%d", devpath, i);
				fd = open(devname, O_RDWR);
				if (fd < 0)
					continue;
			}
		}

		memset(&dinfo, 0, sizeof(dinfo));
		err = ioctl(fd, HIDIOCGDEVINFO, &dinfo);
		if (err < 0 || (int) dinfo.busnum != atoi(devinfo->dev->bus->dirname) ||
				(int) dinfo.devnum != atoi(devinfo->dev->filename)) {
			close(fd);
			continue;
		}

		err = ioctl(fd, HIDIOCINITREPORT, 0);
		if (err < 0) {
			close(fd);
			break;
		}

		err = send_report(fd, rep1, sizeof(rep1));
		if (err < 0) {
			close(fd);
			break;
		}

		err = send_report(fd, rep2, sizeof(rep2));
		if (err < 0) {
			close(fd);
			break;
		}

		err = send_report(fd, rep3, sizeof(rep3));
		close(fd);
		break;
	}

	return err;
}

static int switch_dell(struct device_info *devinfo)
{
	char report[] = { 0x7f, 0x00, 0x00, 0x00 };

	struct usb_dev_handle *handle;
	int err;

	switch (devinfo->mode) {
	case HCI:
		report[1] = 0x13;
		break;
	case HID:
		report[1] = 0x14;
		break;
	}

	handle = usb_open(devinfo->dev);
	if (!handle)
		return -EIO;

	/* Don't need to check return, as might not be in use */
	usb_detach_kernel_driver_np(handle, 0);

	if (usb_claim_interface(handle, 0) < 0) {
		usb_close(handle);
		return -EIO;
	}

	err = usb_control_msg(handle,
		USB_ENDPOINT_OUT | USB_TYPE_CLASS | USB_RECIP_INTERFACE,
			USB_REQ_SET_CONFIGURATION, 0x7f | (0x03 << 8), 0,
						report, sizeof(report), 5000);

	if (err == 0) {
		err = -1;
		errno = EALREADY;
	} else {
		if (errno == ETIMEDOUT)
			err = 0;
	}

	usb_close(handle);

	return err;
}

static int find_device(struct device_info* devinfo)
{
	struct usb_bus *bus;
	struct usb_device *dev;

	usb_find_busses();
	usb_find_devices();

	for (bus = usb_get_busses(); bus; bus = bus->next)
		for (dev = bus->devices; dev; dev = dev->next) {
			if (dev->descriptor.idVendor == devinfo->vendor &&
			    dev->descriptor.idProduct == devinfo->product) {
				devinfo->dev=dev;
				return 1;
			}
		}
	return 0;
}

static void usage(char* error)
{
	if (error)
		fprintf(stderr,"\n%s\n", error);
	else
		printf("hid2hci - Bluetooth HID to HCI mode switching utility\n\n");

	printf("Usage:\n"
		"\thid2hci [options]\n"
		"\n");

	printf("Options:\n"
		"\t-h, --help           Display help\n"
		"\t-q, --quiet          Don't display any messages\n"
		"\t-r, --mode=          Mode to switch to [hid, hci]\n"
		"\t-v, --vendor=        Vendor ID to act upon\n"
		"\t-p, --product=       Product ID to act upon\n"
		"\t-m, --method=        Method to use to switch [csr, logitech, dell]\n"
		"\n");
	if (error)
		exit(1);
}

static struct option main_options[] = {
	{ "help",	no_argument, 0, 'h' },
	{ "quiet",	no_argument, 0, 'q' },
	{ "mode",	required_argument, 0, 'r' },
	{ "vendor",	required_argument, 0, 'v' },
	{ "product",	required_argument, 0, 'p' },
	{ "method",	required_argument, 0, 'm' },
	{ 0, 0, 0, 0 }
};

int main(int argc, char *argv[])
{
	struct device_info dev = { NULL, HCI, 0, 0 };
	int opt, quiet = 0;
	int (*method)(struct device_info *dev) = NULL;

	while ((opt = getopt_long(argc, argv, "+r:v:p:m:qh", main_options, NULL)) != -1) {
		switch (opt) {
		case 'r':
			if (optarg && !strcmp(optarg, "hid"))
				dev.mode = HID;
			else if (optarg && !strcmp(optarg, "hci"))
				dev.mode = HCI;
			else
				usage("ERROR: Undefined radio mode\n");
			break;
		case 'v':
			sscanf(optarg, "%4hx", &dev.vendor);
			break;
		case 'p':
			sscanf(optarg, "%4hx", &dev.product);
			break;
		case 'm':
			if (optarg && !strcmp(optarg, "csr"))
				method = switch_csr;
			else if (optarg && !strcmp(optarg, "logitech"))
				method = switch_logitech;
			else if (optarg && !strcmp(optarg, "dell"))
				method = switch_dell;
			else
				usage("ERROR: Undefined switching method\n");
			break;
		case 'q':
			quiet = 1;
			break;
		case 'h':
			usage(NULL);
		default:
			exit(0);
		}
	}

	if (!quiet && (!dev.vendor || !dev.product || !method))
		usage("ERROR: Vendor ID, Product ID, and Switching Method must all be defined.\n");

	argc -= optind;
	argv += optind;
	optind = 0;

	usb_init();

	if (!find_device(&dev)) {
		if (!quiet)
			fprintf(stderr, "Device %04x:%04x not found on USB bus.\n",
				dev.vendor, dev.product);
		exit(1);
	}

	if (!quiet)
		printf("Attempting to switch device %04x:%04x to %s mode ",
			dev.vendor, dev.product, dev.mode ? "HID" : "HCI");
	fflush(stdout);

	if (method(&dev) < 0 && !quiet)
		printf("failed (%s)\n", strerror(errno));
	else if (!quiet)
		printf("was successful\n");

	return errno;
}
