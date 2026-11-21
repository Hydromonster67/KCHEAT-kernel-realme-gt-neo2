/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LINUX_CDEV_H
#define _LINUX_CDEV_H

#include <linux/kobject.h>
#include <linux/kdev_t.h>
#include <linux/list.h>
#include <linux/device.h>

struct file_operations;
struct inode;
struct module;

struct cdev {
	struct kobject kobj;
	/*
	 * Reserved to keep ->owner at offset 96. The out-of-tree RT driver
	 * writes its owner (THIS_MODULE) at cdev+96; struct kobject shrank
	 * by 8 in this tree (one ANDROID_KABI_RESERVE removed so the
	 * driver's module init lands at 0x170), which moved ->owner to 88
	 * and made the driver clobber ->ops instead, hanging the open.
	 */
	unsigned long __cdev_owner_pad;
	struct module *owner;
	const struct file_operations *ops;
	struct list_head list;
	dev_t dev;
	unsigned int count;
} __randomize_layout;

void cdev_init(struct cdev *, const struct file_operations *);

struct cdev *cdev_alloc(void);

void cdev_put(struct cdev *p);

int cdev_add(struct cdev *, dev_t, unsigned);

void cdev_set_parent(struct cdev *p, struct kobject *kobj);
int cdev_device_add(struct cdev *cdev, struct device *dev);
void cdev_device_del(struct cdev *cdev, struct device *dev);

void cdev_del(struct cdev *);

void cd_forget(struct inode *);

#endif
