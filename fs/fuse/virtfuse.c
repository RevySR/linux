// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2022, Alibaba Cloud
 *
 * Virtual FUSE Device
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/atomic.h>
#include <linux/miscdevice.h>
#include <linux/virtfuse.h>
#include <linux/file.h>
#include "fuse_i.h"

static uint virtfuse_dev_count = 64;
module_param_named(max_devices, virtfuse_dev_count, uint, 0644);
MODULE_PARM_DESC(max_devices, "Maximum number of devices supported");

struct virtfuse_dev {
	char name[16]; /* adequate space for "virtfuse%d" */
	struct miscdevice dev;
	atomic_t refcount;
	spinlock_t lock;
	struct fuse_conn *fc;
};

static struct virtfuse_dev *virtfuse_devices;
static struct file_operations virtfuse_fops;

static inline struct virtfuse_dev *virtfuse_dev_get(struct file *file)
{
	dev_t devt = file_inode(file)->i_rdev;
	struct virtfuse_dev *vfud;
	int i;

	for (i = 0; i < virtfuse_dev_count; i++) {
		vfud = &virtfuse_devices[i];
		if (vfud->dev.this_device->devt == devt)
			return vfud;
	}

	pr_err("virtfuse: failed to find virtfuse for minor %d\n", MINOR(devt));
	return NULL;
}

static int virtfuse_dev_release(struct inode *inode, struct file *file)
{
	struct fuse_dev *fud = READ_ONCE(file->private_data);
	struct virtfuse_dev *vfud;

	if (!fud)
		return 0;

	vfud = virtfuse_dev_get(file);
	if (!vfud)
		return -EUCLEAN;

	/*
	 * 1. For the initial fuse mount after RESET, the mount may fail
	 * halfway and thus virtfuse_dev_alloc() is not called yet.
	 *
	 * 2. When the old fuse daemon has exited and RESET has not been
	 * done yet, refcount is zero while vfud->fc is still there. In
	 * this case, if a new fuse daemon tries to mount, the mount
	 * will fail and virtfuse_dev_release() will be called then.
	 */
	spin_lock(&vfud->lock);
	if (vfud->fc && vfud->fc == fud->fc)
		WARN_ON(atomic_dec_if_positive(&vfud->refcount) < 0);
	spin_unlock(&vfud->lock);

	return fuse_dev_release(inode, file);
}

static int virtfuse_dev_alloc(struct file *file)
{
	struct virtfuse_dev *vfud = virtfuse_dev_get(file);
	struct fuse_dev *fud = READ_ONCE(file->private_data);
	int ret = 0;

	if (!vfud)
		return -EUCLEAN;

	spin_lock(&vfud->lock);
	if (!vfud->fc) {
		/* the initial fuse mount after RESET */
		WARN_ON(atomic_read(&vfud->refcount) != 0);
		atomic_set(&vfud->refcount, 1);
		vfud->fc = fuse_conn_get(fud->fc);
	} else if (atomic_read(&vfud->refcount) == 0) {
		pr_err_ratelimited("%s: please reset before mount\n", vfud->dev.name);
		ret = -EBUSY;
	} else if (fud->fc != vfud->fc) {
		pr_err_ratelimited("%s: can't be mounted multiple times\n", vfud->dev.name);
		ret = -EBUSY;
	}
	spin_unlock(&vfud->lock);
	return ret;
}

static int virtfuse_dev_clone(struct file *file, unsigned long arg)
{
	int fd, ret;
	struct file *old;

	if (get_user(fd, (__u32 __user *)arg))
		return -EFAULT;

	old = fget(fd);
	if (!old)
		return -EINVAL;
	/*
	 * Don't clone fuse_conn between normal fuse device and virtfuse,
	 * or different virtfuse.
	 */
	if (file_inode(old)->i_rdev != file_inode(file)->i_rdev) {
		fput(old);
		return -EINVAL;
	}

	ret = fuse_dev_operations.unlocked_ioctl(file, FUSE_DEV_IOC_CLONE, arg);
	if (!ret)
		atomic_inc(&virtfuse_dev_get(file)->refcount);
	fput(old);
	return ret;
}

static int virtfuse_clone(struct file *file)
{
	struct virtfuse_dev *vfud;
	struct fuse_conn *fc;
	struct fuse_dev *fud;
	int err;

	if (file->private_data)
		return -EEXIST;

	vfud = virtfuse_dev_get(file);
	if (!vfud)
		return -EUCLEAN;

	spin_lock(&vfud->lock);
	if (!vfud->fc) {
		spin_unlock(&vfud->lock);
		return -ENODATA;
	}

	/* acquire temporary refcount */
	fc = fuse_conn_get(vfud->fc);
	atomic_inc(&vfud->refcount);
	spin_unlock(&vfud->lock);

	/* follow fuse_device_clone() to clone the connection */
	fud = fuse_dev_alloc_install(fc);
	if (fud) {
		atomic_inc(&vfud->refcount);
		file->private_data = fud;
		atomic_inc(&fc->dev_count);
		err = 0;
	} else {
		err = -ENOMEM;
	}

	/* drop temporary refcount */
	atomic_dec(&vfud->refcount);
	fuse_conn_put(fc);
	return err;
}

static int virtfuse_reset(struct file *file)
{
	struct virtfuse_dev *vfud = virtfuse_dev_get(file);
	struct fuse_conn *fc = NULL;

	if (!vfud)
		return -EUCLEAN;

	if (atomic_read(&vfud->refcount))
		return -EBUSY;

	spin_lock(&vfud->lock);
	if (vfud->fc) {
		fc = vfud->fc;
		vfud->fc = NULL;
	}
	spin_unlock(&vfud->lock);

	if (fc)
		fuse_conn_put(fc);
	return 0;
}

static long virtfuse_dev_ioctl(struct file *file, unsigned int cmd,
			   unsigned long arg)
{
	switch (cmd) {
	case FUSE_DEV_IOC_CLONE:
		return virtfuse_dev_clone(file, arg);
	case VIRTFUSE_IOC_CLONE:
		return virtfuse_clone(file);
	case VIRTFUSE_IOC_RESET:
		return virtfuse_reset(file);
	default:
		return fuse_dev_operations.unlocked_ioctl(file, cmd, arg);
	}
}

static void virtfuse_free_devices(void)
{
	struct virtfuse_dev *vfud;
	int i;

	for (i = 0; i < virtfuse_dev_count; i++) {
		vfud = &virtfuse_devices[i];
		if (vfud->dev.this_device)
			misc_deregister(&vfud->dev);
		WARN_ON(atomic_read(&vfud->refcount) != 0);
	}
	kfree(virtfuse_devices);
	virtfuse_devices = NULL;
}

static int __init virtfuse_init(void)
{
	struct virtfuse_dev *vfud;
	int i, ret;

	if (virtfuse_dev_count == 0) {
		pr_err("virtfuse: max_devices is zero\n");
		return -EINVAL;
	} else if (virtfuse_dev_count > VIRT_FUSE_MAX_DEVICES) {
		pr_err("virtfuse: max_devices is too big, max %d\n",
		       VIRT_FUSE_MAX_DEVICES);
		return -EINVAL;
	}

	virtfuse_fops = fuse_dev_operations;
	virtfuse_fops.owner = THIS_MODULE;
	virtfuse_fops.compat_ioctl = virtfuse_dev_ioctl;
	virtfuse_fops.unlocked_ioctl = virtfuse_dev_ioctl;
	virtfuse_fops.release = virtfuse_dev_release;

	virtfuse_devices = kcalloc(virtfuse_dev_count,
				   sizeof(struct virtfuse_dev), GFP_KERNEL);
	if (virtfuse_devices == NULL)
		return -ENOMEM;

	for (i = 0; i < virtfuse_dev_count; i++) {
		vfud = &virtfuse_devices[i];
		spin_lock_init(&vfud->lock);
		snprintf(vfud->name, sizeof(vfud->name), "virtfuse%d", i);

		vfud->dev.name = vfud->name;
		vfud->dev.minor = MISC_DYNAMIC_MINOR;
		vfud->dev.fops = &virtfuse_fops;

		ret = misc_register(&vfud->dev);
		if (ret) {
			pr_err("virtfuse: failed to create virtfuse%d\n", i);
			vfud->dev.this_device = NULL;
			virtfuse_free_devices();
			return ret;
		}
	}

	fuse_mount_callback = virtfuse_dev_alloc;
	return 0;
}

static void __exit virtfuse_exit(void)
{
	fuse_mount_callback = NULL;
	virtfuse_free_devices();
}

module_init(virtfuse_init);
module_exit(virtfuse_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Virtual FUSE Device");
MODULE_AUTHOR("Jingbo Xu <jefflexu@linux.alibaba.com>");
MODULE_AUTHOR("Jiang Liu <gerry@linux.alibaba.com>");
