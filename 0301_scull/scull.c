#include <linux/init.h>
#include <linux/module.h>
#include <linux/kdev_t.h> /* MAJOR() */
                          /* MKDEV() */
#include <linux/fs.h> /* struct file_operations */
                      /* struct file */
                      /* alloc_chrdev_region() */
#include <linux/cdev.h> /* struct cdev */
#include <linux/slab.h> /* kmalloc() */
                        /* kfree() */
#include <asm/uaccess.h> /* copy_to_user() */

struct scull_qset
{
    void **data;
    struct scull_qset *next;
};

struct scull_dev
{
    struct scull_qset *data;
    int quantum; // 每个 quantum 大小（4000 字节）
    int qset; // quantum 数组大小
    unsigned long size; // 
    struct cdev cdev;
};

int scull_major = 0;
int scull_minor = 0;
int scull_nr_devices = 4;
int scull_quantum = 4000; // byte
int scull_qset = 1000; // piece

MODULE_LICENSE("Dual BSD/GPL");

struct scull_dev *scull_devices;

static int scull_trim(struct scull_dev *dev)
{
    struct scull_qset *next, *dptr;
    int qset = dev->qset;
    int i;

    for (dptr = dev->data; dptr; dptr = next)
    {
        if (dptr->data)
        {
            for (i = 0; i < qset; i++)
                kfree(dptr->data[i]);
            kfree(dptr->data);
            dptr->data = NULL;
        }

        next = dptr->next;
        kfree(dptr);
    }

    dev->size = 0;
    dev->quantum = scull_quantum;
    dev->qset = scull_qset;
    dev->data = NULL;

    return 0;
}

struct scull_qset *scull_follow(struct scull_dev *dev, int n)
{
    struct scull_qset *qs = dev->data;

    if (NULL == qs)
    {
        qs = dev->data = kmalloc(sizeof(struct scull_qset), GFP_KERNEL);
        if (NULL == qs)
            return NULL;
        memset(qs, 0, sizeof(struct scull_qset));
    }

    while (n--)
    {
        if (NULL == qs->next)
        {
            qs = kmalloc(sizeof(struct scull_qset), GFP_KERNEL);
            if (NULL == qs)
                return NULL;
            memset(qs, 0, sizeof(struct scull_qset));
        }

        qs = qs->next;
    }

    return qs;
}

ssize_t scull_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
    struct scull_dev *dev = filp->private_data;
    struct scull_qset *dptr;
    int quantum = dev->quantum;
    int qset = dev->qset;
    int itemsize = quantum * qset;
    int item, s_pos, q_pos, rest;
    ssize_t retval = 0;

    printk(KERN_ALERT "scull_read");
    printk(KERN_ALERT "qset = %d; count = %d; f_pos = %d", qset, count, *f_pos);
    printk(KERN_ALERT "dev->size = %ld", dev->size);

    if (*f_pos >= dev->size)
        goto out;
    if (*f_pos + count > dev->size)
        count = dev->size - *f_pos;

    item = (long)*f_pos / itemsize; // f_pos 中有多少个 scull_qset
    rest = (long)*f_pos % itemsize; // 再加多少个字节
    s_pos = rest / quantum; // rest 中有多少个 quantum
    q_pos = rest % quantum; // 再加多少个字节

    dptr = scull_follow(dev, item); // 找到链表中第 item 个 scull_qset

    if (NULL == dptr || NULL == dptr->data || NULL == dptr->data[s_pos])
        goto out;

    if (count > quantum - q_pos)
        count = quantum - q_pos;

    if (copy_to_user(buf, dptr->data[s_pos] + q_pos, count))
    {
        retval = -EFAULT;
        goto out;
    }
    printk(KERN_INFO "buf = %s", buf);
    *f_pos += count;
    retval = count;

  out:
    return retval;
}

ssize_t scull_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos)
{
    struct scull_dev *dev = filp->private_data;
    struct scull_qset *dptr = NULL;
    int quantum = dev->quantum;
    int qset = dev->qset;
    int itemsize = quantum * qset;
    int item, s_pos, q_pos, rest;
    ssize_t retval = 0;

    printk(KERN_ALERT "scull_write");
    printk(KERN_ALERT "qset = %d; count = %d; f_pos = %d", qset, count, *f_pos);

    item = (long)f_pos / itemsize;
    rest = (long)f_pos % itemsize;
    s_pos = rest / quantum;
    q_pos = rest % quantum;

    dptr = scull_follow(dev, item);

    if (NULL == dptr)
        goto out;
    if (NULL == dptr->data)
    {
        dptr->data = kmalloc(qset * sizeof(char *), GFP_KERNEL);
        if (NULL == dptr->data)
            goto out;
        memset(dptr->data, 0, qset * sizeof(char *));
    }
    if (NULL == dptr->data[s_pos])
    {
        dptr->data[s_pos] = kmalloc(scull_quantum, GFP_KERNEL);
        if (NULL == dptr->data[s_pos])
            goto out;
    }

    if (count > quantum - q_pos)
        count = quantum - q_pos;

    if (copy_from_user(dptr->data[s_pos] + q_pos, buf, count))
    {
        retval = -EFAULT;
        goto out;
    }
    *f_pos += count;
    retval = count;

    if (dev->size < *f_pos)
        dev->size = *f_pos;

  out:
    return retval;
}

int scull_open(struct inode *inode, struct file *filp)
{
    struct scull_dev *dev;

    printk(KERN_ALERT "scull_open");

    dev = container_of(inode->i_cdev, struct scull_dev, cdev);
    filp->private_data = dev;

    /* 如果以只写（创建）方式打开，则将数据清零 */
    if (O_WRONLY == (filp->f_flags & O_ACCMODE))
        scull_trim(dev);

    return 0;
}

int scull_release(struct inode *inode, struct file *filp)
{
    printk(KERN_ALERT "scull_release");
    return 0;
}

struct file_operations scull_fops =
{
    .owner = THIS_MODULE,
    .read = scull_read,
    .write = scull_write,
    .open = scull_open,
    .release = scull_release,
};

static void __exit scull_exit_module(void)
{
    dev_t devno = MKDEV(scull_major, scull_minor);
    int i = 0;

    if (NULL != scull_devices)
    {
        for (i = 0; i < scull_nr_devices; i++)
        {
            scull_trim(&scull_devices[i]);
            cdev_del(&scull_devices[i].cdev);
        }
    }

    unregister_chrdev_region(devno, scull_nr_devices);
}

static void scull_setup_cdev(struct scull_dev *dev, int index)
{
    int err;
    dev_t devno = MKDEV(scull_major, scull_minor + index);

    /* 初始化 cdev */
    cdev_init(&dev->cdev, &scull_fops);
    dev->cdev.owner = THIS_MODULE;
    dev->cdev.ops = &scull_fops;

    /* 注册 cdev */
    err = cdev_add(&dev->cdev, devno, 1);

    if (err)
        printk(KERN_NOTICE "Error %d adding scull%d", err, index);
}

static int __init scull_init_module(void)
{
    int result = -1;
    dev_t dev = 0;
    int i = 0;

    /* 动态分配主设备号 */
    result = alloc_chrdev_region(&dev, scull_minor, scull_nr_devices, "scull");
    scull_major = MAJOR(dev); // 获得 dev_t 的主设备号
    if (0 > result) // 主设备号分配失败
    {
        printk(KERN_ALERT "scull cannot get major %d\n", scull_major);
        return result;
    }

    /* 为设备分配内存空间并初始化 */
    scull_devices = kmalloc(scull_nr_devices * sizeof(struct scull_dev), GFP_KERNEL);
    if (NULL == scull_devices)
    {
        printk(KERN_ALERT "kmalloc(%d * %ld) failed\n", scull_nr_devices, sizeof(struct scull_dev));
        result = -ENOMEM;
        goto fail;
    }
    memset(scull_devices, 0, scull_nr_devices * sizeof(struct scull_dev));

    /* 依次初始化每个设备 */
    for (i = 0; i < scull_nr_devices; i++)
    {
        printk(KERN_INFO "Init module: scull%d-%d\n", scull_major, i);

        scull_devices[i].quantum = scull_quantum;
        scull_devices[i].qset = scull_qset;

        scull_setup_cdev(&scull_devices[i], i);
    }

    return 0;

  fail:
    scull_exit_module();
    return result;
}

module_init(scull_init_module);
module_exit(scull_exit_module);
