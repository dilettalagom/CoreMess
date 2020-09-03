#define EXPORT_SYMTAB
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/pid.h>		/* For pid types */
#include <linux/tty.h>		/* For the tty declarations */
#include <linux/version.h>	/* For LINUX_VERSION_CODE */
#include <linux/uaccess.h>  /* copy_to/from_user */
#include <linux/workqueue.h> /* work_queus */
#include <linux/jiffies.h>
#include  <asm/param.h> /* for HZ */


#include "../include/config.h"
#include "../include/new_structures.h"

#define DEBUG if(1) //enable debug printk

#define MODNAME "CORE_MESS"
#define DEVICE_NAME "core_mess_dev"

//general conf
#define MINORS 3 //MAX_DEV_INSTANCES
#define PERM_MODE S_IWUSR | S_IRUGO  //(0644) read-only: S_IRUSR | S_IRGRP | S_IROTH


//dev_op
static int dev_open(struct inode *, struct file *);
static int dev_release(struct inode *, struct file *);
static ssize_t dev_read(struct file *, char *, size_t, loff_t *);
static ssize_t dev_write(struct file *, const char *, size_t, loff_t *);
static long dev_ioctl(struct file *, unsigned int, unsigned long);
static int dev_flush(struct file *, fl_owner_t );


