/**
 * @file aesdchar.c
 * @brief Functions and data related to the AESD char driver implementation
 *
 * Based on the implementation of the "scull" device driver, found in
 * Linux Device Drivers example code.
 *
 * @author Dan Walkes
 * @author Xiang Guan Deng
 * @date 2019-10-22
 * @copyright Copyright (c) 2019
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/printk.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/fs.h> // file_operations
#include<linux/slab.h>
#include <linux/mutex.h>
#include "aesd_ioctl.h"
#include "aesdchar.h"

/* Circular buffer */
#include "aesd-circular-buffer.h"

int aesd_major =   0; // use dynamic major
int aesd_minor =   0;

static DEFINE_MUTEX(aesd_lock);

MODULE_AUTHOR("Xiang Guan Deng");
MODULE_LICENSE("Dual BSD/GPL");

static struct aesd_dev aesd_device = {.is_open_ = false};

int aesd_open(struct inode *inode, struct file *filp)
{
    PDEBUG("open");
    if(aesd_device.is_open_) {
        // device is already open
        return -EBUSY;
    }
    aesd_device.is_open_ = true;

    return 0;
}

int aesd_release(struct inode *inode, struct file *filp)
{
    PDEBUG("release");
    aesd_device.is_open_ = false;
    return 0;
}

loff_t aesd_llseek(struct file *filp, loff_t offset, int whence) {
    PDEBUG("f_pos: %lu, offset: %ld, whence: %d", filp->f_pos, offset, whence);
    int i;

    switch (whence) {
    case SEEK_SET:
        filp->f_pos = 0;
        for(i=0;i<offset&&i<AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;i++) {
            filp->f_pos += aesd_device.cir_buf_.entry[i].size;
        }
        PDEBUG("SEEK_SET: %ld\n", filp->f_pos);
        break;
    case SEEK_CUR:
        filp->f_pos += offset;
        break;
    case SEEK_END:
        break;
    default:
        return -EINVAL;
    }

    return filp->f_pos;
}

long aesd_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    PDEBUG("cmd: %lu, arg: %ld", cmd, arg);

    /* Return -EINVAL if parameters out of range */

    return 0;
}

ssize_t aesd_read(struct file *filp, char __user *buf, size_t count,
                loff_t *f_pos)
{
    if(!aesd_device.is_open_) {
        return -1;
    }
    if(aesd_device.index_ == aesd_device.count_) {
        aesd_device.index_ = 0;
        return 0;
    }
    ssize_t retval = 0;
    PDEBUG("read %zu bytes with offset %lld",count,*f_pos);

    size_t offset_rtn = 0;
    aesd_device.cir_buf_.out_offs;
    ssize_t char_offset = *f_pos;

    struct aesd_buffer_entry *rtnentry;

    mutex_lock(&aesd_lock);
    rtnentry = aesd_circular_buffer_find_entry_offset_for_fpos(&aesd_device.cir_buf_,
                                                char_offset,
                                                &offset_rtn);
    if(NULL == rtnentry) {
        mutex_unlock(&aesd_lock);
        return 0;
    }
    ssize_t ret_sz = 0;
    PDEBUG("rtentry: %p, %u", rtnentry, char_offset);
    if(rtnentry) {
        PDEBUG("rtentry: %s, %d, %d", rtnentry->buffptr, rtnentry->size, offset_rtn);

        ret_sz = rtnentry->size-offset_rtn;
        copy_to_user(buf, &rtnentry->buffptr[offset_rtn], ret_sz);
        char_offset += ret_sz;
    }
    mutex_unlock(&aesd_lock);
    *f_pos = char_offset;

    return ret_sz;
}

ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count,
                loff_t *f_pos)
{
    if(!aesd_device.is_open_) {
        return -1;
    }
    PDEBUG("write %zu bytes with offset %lld",count,*f_pos);
    int i;

    if('\n' != buf[count-1]) {
        if(aesd_device.last_malloc_buf_) {
            aesd_device.last_malloc_buf_ = krealloc(aesd_device.last_malloc_buf_, aesd_device.last_malloc_sz_ + count, GFP_KERNEL);
            memcpy(&aesd_device.last_malloc_buf_[aesd_device.last_malloc_sz_], buf, count*sizeof(char));
            aesd_device.last_malloc_sz_ += count * sizeof(char);
        }
        else {
            aesd_device.last_malloc_buf_ = kmalloc(count * sizeof(char), GFP_KERNEL);
            memcpy(&aesd_device.last_malloc_buf_[aesd_device.last_malloc_sz_], buf, count*sizeof(char));
            aesd_device.last_malloc_sz_ = count * sizeof(char);
        }
        return count;
    }

    mutex_lock(&aesd_lock);
    if(0 == count) {
        mutex_unlock(&aesd_lock);
        return 0;
    }


    if(aesd_device.cir_buf_.full) {
        // alread full, free the memory that will overlap
        PDEBUG("buf: %s\n", aesd_device.cir_buf_.entry[aesd_device.cir_buf_.in_offs].buffptr);
        kfree(aesd_device.cir_buf_.entry[aesd_device.cir_buf_.in_offs].buffptr);
        aesd_device.cir_buf_.entry[aesd_device.cir_buf_.in_offs].size = 0;
    }

    struct aesd_buffer_entry entry;

    char *malloc_buf = NULL;
    size_t buf_sz = 0;
    if(NULL != aesd_device.last_malloc_buf_) {
        malloc_buf = aesd_device.last_malloc_buf_;
        aesd_device.last_malloc_buf_ = krealloc(aesd_device.last_malloc_buf_, aesd_device.last_malloc_sz_ + count + 1, GFP_KERNEL);
        memcpy(&aesd_device.last_malloc_buf_[aesd_device.last_malloc_sz_], buf, count*sizeof(char));
        aesd_device.last_malloc_sz_ += count * sizeof(char);
        buf_sz = aesd_device.last_malloc_sz_;
    }
    else {
        malloc_buf = kmalloc(count * sizeof(char) + 1, GFP_KERNEL);
        memcpy(malloc_buf, buf, count*sizeof(char));
        buf_sz = count;
    }
    if(NULL == malloc_buf) {
        PDEBUG("can not malloc\n");
        mutex_unlock(&aesd_lock);
        return -ENOMEM;
    }
    malloc_buf[buf_sz] = '\0';
    entry.size = buf_sz;
    entry.buffptr = malloc_buf;
    aesd_circular_buffer_add_entry(&aesd_device.cir_buf_, &entry);
    // printk(KERN_INFO "%s", malloc_buf);
    mutex_unlock(&aesd_lock);
    if(aesd_device.count_ < AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED)
    {
        aesd_device.count_++;
    }
    
    aesd_device.last_malloc_buf_ = NULL;
    aesd_device.last_malloc_sz_ = 0;

    /* Print out buffer */
#ifdef AESD_DEBUG
    for(i=0;i<AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;i++)
    {
        PDEBUG("[%d]: %s, %p, %d, %d, %d, %d", i, aesd_device.cir_buf_.entry[i].buffptr, aesd_device.cir_buf_.entry[i].buffptr, aesd_device.cir_buf_.entry[i].size, 
                        aesd_device.cir_buf_.in_offs, aesd_device.cir_buf_.out_offs, aesd_device.cir_buf_.full);
    }
#endif /* AESD_DEBUG */
    
    return count;
}
struct file_operations aesd_fops = {
    .owner =    THIS_MODULE,
    .llseek =   aesd_llseek,
    .unlocked_ioctl = aesd_ioctl,
    .read =     aesd_read,
    .write =    aesd_write,
    .open =     aesd_open,
    .release =  aesd_release,
};

static int aesd_setup_cdev(struct aesd_dev *dev)
{
    int err, devno = MKDEV(aesd_major, aesd_minor);

    cdev_init(&dev->cdev, &aesd_fops);
    dev->cdev.owner = THIS_MODULE;
    dev->cdev.ops = &aesd_fops;
    err = cdev_add (&dev->cdev, devno, 1);
    if (err) {
        printk(KERN_ERR "Error %d adding aesd cdev", err);
    }
    return err;
}



int aesd_init_module(void)
{
    PDEBUG("aesd init module");
    dev_t dev = 0;
    int result;
    result = alloc_chrdev_region(&dev, aesd_minor, 1,
            "aesdchar");
    aesd_major = MAJOR(dev);
    if (result < 0) {
        printk(KERN_WARNING "Can't get major %d\n", aesd_major);
        return result;
    }
    memset(&aesd_device,0,sizeof(struct aesd_dev));

    /**
     * TODO: initialize the AESD specific portion of the device
     */
    aesd_circular_buffer_init(&aesd_device.cir_buf_);

    result = aesd_setup_cdev(&aesd_device);

    if( result ) {
        unregister_chrdev_region(dev, 1);
    }
    return result;

}

void aesd_cleanup_module(void)
{
    PDEBUG("aesd clean module");
    dev_t devno = MKDEV(aesd_major, aesd_minor);

    cdev_del(&aesd_device.cdev);

    /**
     * TODO: cleanup AESD specific poritions here as necessary
     */

    unregister_chrdev_region(devno, 1);
}



module_init(aesd_init_module);
module_exit(aesd_cleanup_module);
