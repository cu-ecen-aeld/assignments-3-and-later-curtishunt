/**
 * @file aesdchar.c
 * @brief Functions and data related to the AESD char driver implementation
 *
 * Based on the implementation of the "scull" device driver, found in
 * Linux Device Drivers example code.
 *
 * @author Dan Walkes
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
#include "aesdchar.h"
int aesd_major =   0; // use dynamic major
int aesd_minor =   0;

MODULE_AUTHOR("Your Name Here"); /** TODO: fill in your name **/
MODULE_LICENSE("Dual BSD/GPL");

struct aesd_dev aesd_device;

int aesd_open(struct inode *inode, struct file *filp)
{
    PDEBUG("open");
    /**
     * TODO: handle open
     */
    filp->private_data = &aesd_device;
    return 0;
}

int aesd_release(struct inode *inode, struct file *filp)
{
    PDEBUG("release");
    /**
     * TODO: handle release
     */
    return 0;
}

ssize_t aesd_read(struct file *filp, char __user *buf, size_t count,
                loff_t *f_pos)
{
    ssize_t retval = 0;
    ssize_t total_copied = 0;
    PDEBUG("read %zu bytes with offset %lld",count,*f_pos);
    /**
     * TODO: handle read
     */
    
    struct aesd_buffer_entry *entry;
    size_t entry_offset;

    mutex_lock(&aesd_device.lock);

    while (count > 0){
        entry = aesd_circular_buffer_find_entry_offset_for_fpos(&aesd_device.buffer, *f_pos, &entry_offset);
        if (!entry) {
            break;
        }

        // amount of bytes in entry that are desired
        size_t bytes_available = entry->size - entry_offset;
        // amount of bytes
        size_t bytes_to_copy = min(count, bytes_available);

        if (copy_to_user(buf, entry->buffptr + entry_offset, bytes_to_copy)) {
            retval = -EFAULT;
            goto out;
        }

        *f_pos += bytes_to_copy;
        count -= bytes_to_copy;
        total_copied += bytes_to_copy;
        buf += bytes_to_copy;
    }   
    retval = total_copied;

    out:
        mutex_unlock(&aesd_device.lock);
        return retval;
}

ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count,
                loff_t *f_pos)
{
    ssize_t retval = -ENOMEM;
    PDEBUG("write %zu bytes with offset %lld",count,*f_pos);
    
    //debug//
    printk(KERN_DEBUG "starting aesd_write\n");
    // for (size_t j=0; j<count; j++){
    //     printk(KERN_DEBUG "buf input %zu: %c\n", j, buf[j]);
    // }
    // end debug //

    /**
     * TODO: handle write
     */
    retval = count;
    char *kbuf;
    size_t i;
    // from aesd-circular-buffer.h
    struct aesd_buffer_entry entry;
    const char *freed = NULL;

    if (count == 0)
        return 0;

    // Allocate mem for kbuf GFP_KERNEL = normal kernel ram, this function may sleep!
    kbuf = kmalloc(count, GFP_KERNEL);
    if (!kbuf)
        return -ENOMEM;

    // Copy buf to kbuf, will return 0 if successful
    if (copy_from_user(kbuf, buf, count)) {
        kfree(kbuf);
        return -EFAULT;
    }

    mutex_lock(&aesd_device.lock);

    // Expand existing partial write if present
    char *combined = NULL;
    if (aesd_device.partial) {
        //allocate mem for a buffer big enought to hold new data and old partial data
        combined = kmalloc(aesd_device.partial_size + count, GFP_KERNEL);
        if (!combined) {
            kfree(kbuf);
            mutex_unlock(&aesd_device.lock);
            return -ENOMEM;
        }
        //copy previous partial data to buffer "combined"
        memcpy(combined, aesd_device.partial, aesd_device.partial_size);
        //copy new data to combined at the end of the partial data
        memcpy(combined + aesd_device.partial_size, kbuf, count);
        // get rid of partial data containers
        kfree(aesd_device.partial);
        kfree(kbuf);
        // this is effectively renaming combined to kbuf
        kbuf = combined;
        count += aesd_device.partial_size;
        aesd_device.partial = NULL;
        aesd_device.partial_size = 0;
    }

    // Look for newline
    for (i = 0; i < count; i++) {
        if (kbuf[i] == '\n')
            break;
    }
    // "i" now has the length of kbuf, if there was no \n char then i==count
    if (i < count) {
        // We have a full command ending in '\n'
        entry.buffptr = kmalloc(i + 1, GFP_KERNEL); // use i+1, only copy up to and including \n
        if (!entry.buffptr) {
            kfree(kbuf);
            mutex_unlock(&aesd_device.lock);
            return -ENOMEM;
        }
        memcpy(entry.buffptr, kbuf, i + 1);
        entry.size = i + 1;

        aesd_circular_buffer_add_entry(&aesd_device.buffer, &entry);

        printk(KERN_INFO "Added entry: %.*s", (int)entry.size, entry.buffptr);
        printk(KERN_INFO "Buffer in_offs: %d, out_offs: %d, full: %d\n", aesd_device.buffer.in_offs, aesd_device.buffer.out_offs, aesd_device.buffer.full);
        uint8_t entries_count = aesd_device.buffer.full ? AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED : aesd_device.buffer.in_offs;
        printk(KERN_INFO "Entries in buffer: %d\n", entries_count);

        // Save leftover data after newline (if any)
        size_t remaining = count - (i + 1);
        if (remaining > 0) {
            aesd_device.partial = kmalloc(remaining, GFP_KERNEL);
            if (!aesd_device.partial) {
                kfree(kbuf);
                mutex_unlock(&aesd_device.lock);
                return -ENOMEM;
            }
            memcpy(aesd_device.partial, kbuf + i + 1, remaining);
            aesd_device.partial_size = remaining;
        }

        kfree(kbuf);
    } else {
        // Incomplete write: store in partial
        aesd_device.partial = kbuf;
        aesd_device.partial_size = count;
    }

    mutex_unlock(&aesd_device.lock);

    return retval;
}

loff_t aesd_llseek(struct file *filp, loff_t offset, int whence)
{
    loff_t new_pos = 0;
    struct aesd_dev *dev = filp->private_data;
    loff_t curr_size = 0;
    uint8_t index;

    mutex_lock(&dev->lock);

    // Compute total size across all valid entries
    for (index = 0; index < AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED; index++) {
        if (dev->buffer.entry[index].buffptr)
            curr_size += dev->buffer.entry[index].size;
    }

    switch (whence) {
        case SEEK_SET:
            new_pos = offset;
            break;
        case SEEK_CUR:
            new_pos = filp->f_pos + offset;
            break;
        case SEEK_END:
            new_pos = curr_size + offset; // offset sould be < 0 in this case
            break;
        default:
            mutex_unlock(&dev->lock);
            return -EINVAL;
    }

    // check if we went out of the bounds of our current buffer size
    if (new_pos < 0 || new_pos > curr_size) {
        mutex_unlock(&dev->lock);
        return -EINVAL;
    }

    filp->f_pos = new_pos;
    mutex_unlock(&dev->lock);

    return new_pos;
}

struct file_operations aesd_fops = {
    .owner =    THIS_MODULE,
    .read =     aesd_read,
    .write =    aesd_write,
    .open =     aesd_open,
    .release =  aesd_release,
    .llseek = aesd_llseek
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
    dev_t dev = 0;
    int result;
    result = alloc_chrdev_region(&dev, aesd_minor, 1,
            "aesdchar");
    aesd_major = MAJOR(dev);
    
    if (result < 0) {
        printk(KERN_WARNING "Can't get major %d\n", aesd_major);
        return result;
    }

    // adding for debug
    printk(KERN_INFO "aesdchar: registered with major number %d\n", aesd_major);

    memset(&aesd_device,0,sizeof(struct aesd_dev));

    /**
     * TODO: initialize the AESD specific portion of the device
     */
    mutex_init(&aesd_device.lock);
    aesd_circular_buffer_init(&aesd_device.buffer);
    aesd_device.partial = NULL;
    aesd_device.partial_size = 0;

    result = aesd_setup_cdev(&aesd_device);

    if( result ) {
        unregister_chrdev_region(dev, 1);
    }
    return result;

}

void aesd_cleanup_module(void)
{
    dev_t devno = MKDEV(aesd_major, aesd_minor);

    cdev_del(&aesd_device.cdev);

    /**
     * TODO: cleanup AESD specific poritions here as necessary
     */

    unregister_chrdev_region(devno, 1);
    uint8_t i;
    for (i = 0; i < AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED; i++) {
        if (aesd_device.buffer.entry[i].buffptr)
            kfree(aesd_device.buffer.entry[i].buffptr);
    }
    if (aesd_device.partial)
        kfree(aesd_device.partial);
}



module_init(aesd_init_module);
module_exit(aesd_cleanup_module);
