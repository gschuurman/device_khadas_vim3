/*
 * Watches the kernel uevent netlink socket for the Ugreen/AIC8800D80 USB
 * dongle's decoy mass-storage enumeration (idVendor=a69c, idProduct=5723)
 * and issues a SCSI eject, which flips it into its real WiFi-mode
 * enumeration (a69c:8d80). Android's ueventd has no udev-style RUN+=
 * action support, so this reimplements the equivalent udev rule as a
 * persistent daemon instead.
 *
 * The raw kernel uevent stream reports the USB device and its resulting
 * block device as two separate events with no field linking them, so this
 * tracks the DEVPATH of the most recently seen matching USB device and
 * ejects the next block-disk event whose DEVPATH nests under it.
 */

#include <errno.h>
#include <fcntl.h>
#include <linux/netlink.h>
#include <scsi/sg.h>
#include <selinux/selinux.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

#define LOG_TAG "aic8800_modeswitch"
#include <android/log.h>

#define ALOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define ALOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

#define TARGET_PRODUCT_PREFIX "a69c/5723/" /* PRODUCT=idVendor/idProduct/bcdDevice, hex, no leading zeros */
#define UEVENT_BUF_SIZE 8192
#define DEVPATH_MAX 256

#define AIC8800_BLOCK_DEVICE_CONTEXT "u:object_r:aic8800_block_device:s0"

/* SCSI START STOP UNIT (0x1B) with the eject bit set -- what `eject` uses
 * for USB mass-storage devices that aren't real optical drives. */
static int scsi_eject(const char *devpath) {
    /* Hotplugged block nodes get the generic "block_device" catch-all
     * type (the sdX letter is dynamic, so it can't be pinned in
     * file_contexts). domain.te neverallows raw blk_file access on that
     * type outside a few core domains, so relabel to our own narrow type
     * before opening it. */
    if (setfilecon(devpath, AIC8800_BLOCK_DEVICE_CONTEXT) < 0) {
        ALOGE("setfilecon(%s) failed: %s", devpath, strerror(errno));
        return -1;
    }

    /* O_RDONLY: the decoy device reports itself as write-protected at the
     * SCSI level, so O_RDWR fails with EROFS. SG_IO with SG_DXFER_NONE
     * doesn't need a writable fd. */
    int fd = open(devpath, O_RDONLY | O_NONBLOCK);
    if (fd < 0) {
        ALOGE("open(%s) failed: %s", devpath, strerror(errno));
        return -1;
    }

    unsigned char cdb[6] = {0x1B, 0, 0, 0, 0x02, 0}; /* START STOP UNIT, LoEj=1, Start=0 */
    unsigned char sense[32] = {0};
    sg_io_hdr_t io;
    memset(&io, 0, sizeof(io));
    io.interface_id = 'S';
    io.dxfer_direction = SG_DXFER_NONE;
    io.cmd_len = sizeof(cdb);
    io.cmdp = cdb;
    io.sbp = sense;
    io.mx_sb_len = sizeof(sense);
    io.timeout = 5000;

    int rc = ioctl(fd, SG_IO, &io);
    close(fd);
    if (rc < 0) {
        ALOGE("SG_IO eject on %s failed: %s", devpath, strerror(errno));
        return -1;
    }
    ALOGI("mode-switch eject issued on %s", devpath);
    return 0;
}

int main(void) {
    int sock = socket(PF_NETLINK, SOCK_DGRAM | SOCK_CLOEXEC, NETLINK_KOBJECT_UEVENT);
    if (sock < 0) {
        ALOGE("socket() failed: %s", strerror(errno));
        return 1;
    }

    struct sockaddr_nl addr;
    memset(&addr, 0, sizeof(addr));
    addr.nl_family = AF_NETLINK;
    addr.nl_pid = getpid();
    addr.nl_groups = 1; /* kernel uevent multicast group */
    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        ALOGE("bind() failed: %s", strerror(errno));
        return 1;
    }

    ALOGI("watching for AIC8800 mode-switch device PRODUCT=%s*", TARGET_PRODUCT_PREFIX);

    char pending_usb_devpath[DEVPATH_MAX] = {0};
    char buf[UEVENT_BUF_SIZE];

    for (;;) {
        int len = recv(sock, buf, sizeof(buf) - 1, 0);
        if (len <= 0) continue;
        buf[len] = '\0';

        const char *action = NULL, *devpath = NULL, *devname = NULL;
        const char *subsystem = NULL, *devtype = NULL, *product = NULL;

        for (const char *p = buf; p < buf + len;) {
            if (!strncmp(p, "ACTION=", 7)) {
                action = p + 7;
            } else if (!strncmp(p, "DEVPATH=", 8)) {
                devpath = p + 8;
            } else if (!strncmp(p, "DEVNAME=", 8)) {
                devname = p + 8;
            } else if (!strncmp(p, "SUBSYSTEM=", 10)) {
                subsystem = p + 10;
            } else if (!strncmp(p, "DEVTYPE=", 8)) {
                devtype = p + 8;
            } else if (!strncmp(p, "PRODUCT=", 8)) {
                product = p + 8;
            }
            p += strlen(p) + 1;
        }

        if (!action || strcmp(action, "add") != 0 || !devpath) continue;

        if (subsystem && !strcmp(subsystem, "usb") && product &&
            !strncmp(product, TARGET_PRODUCT_PREFIX, strlen(TARGET_PRODUCT_PREFIX))) {
            snprintf(pending_usb_devpath, sizeof(pending_usb_devpath), "%s", devpath);
            ALOGI("matched USB device at %s, awaiting its block device", devpath);
            continue;
        }

        if (subsystem && !strcmp(subsystem, "block") && devtype && !strcmp(devtype, "disk") &&
            devname && pending_usb_devpath[0] &&
            !strncmp(devpath, pending_usb_devpath, strlen(pending_usb_devpath))) {
            char blockdev[64];
            /* ueventd creates block device nodes under /dev/block/, not
             * bare /dev/ -- the kernel's own DEVNAME uevent field is just
             * "sda" with no prefix. */
            snprintf(blockdev, sizeof(blockdev), "/dev/block/%s", devname);
            /* usb-storage needs a beat to finish registering the block
             * device node before it'll accept SG_IO commands. */
            usleep(300000);
            scsi_eject(blockdev);
            pending_usb_devpath[0] = '\0';
        }
    }
}
