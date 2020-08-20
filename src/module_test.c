#include "module_test.h"


static unsigned long max_message_size = MAX_MESSAGE_SIZE;
module_param(max_message_size, ulong, PERM_MODE);

static unsigned long max_storage_size = MAX_STORAGE_SIZE;
module_param(max_storage_size, ulong, PERM_MODE);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 0, 0)
#define get_major(session)	MAJOR(session->f_inode->i_rdev)
#define get_minor(session)	MINOR(session->f_inode->i_rdev)
#else
#define get_major(session)	MAJOR(session->f_dentry->d_inode->i_rdev)
#define get_minor(session)	MINOR(session->f_dentry->d_inode->i_rdev)
#endif

//Major number assigned to broadcast device driver
static int Major;
static device_instance instance_by_minor[MINORS];


/* the actual driver */
static int dev_open(struct inode *inode, struct file *file) {


    single_session* session;
    int minor = get_minor(file);

    if(minor >= MINORS){
        return -ENODEV;
    }
    printk("%s: start opening object with minor %d\n",MODNAME,minor);

    //Init single_session metadata
    session = kmalloc(sizeof(single_session), GFP_KERNEL);
    if(session == NULL){
        printk(KERN_ERR "%s: FAIL kmalloc in dev_open\n",MODNAME);
        return -ENOMEM; /* Out of memory */
    }
    DEBUG
        printk("%s: kmalloc succeded\n",MODNAME);

    session->write_timer = 0;
    session->read_timer = 0;


    file->private_data = (void *) session;
    printk("%s: device file successfully opened for object with minor %d\n",MODNAME,minor);

    //device opened by a default nop
    return 0;

    //return -EBUSY;

}


static int dev_release(struct inode *inode, struct file *file) {

    int minor;
    minor = get_minor(file);

    //mutex_unlock(&(device_instance[minor].object_busy));


    printk("%s: device file closed\n",MODNAME);

    //device closed by default nop
    return 0;

}



static ssize_t dev_write(struct file *file, const char *buff, size_t len, loff_t *off) {

    int minor = get_minor(file);
    int ret=0;
    DEBUG
        printk("%s: somebody called a WRITE on dev with minor number %d\n", MODNAME, minor);

    /*   object_state *the_object;

   the_object = device_instance + minor;
   printk("%s: somebody called a write on dev with [major,minor] number [%d,%d]\n",MODNAME,get_major(filp),get_minor(filp));


   //need to lock in any case
   mutex_lock(&(the_object->operation_synchronizer));
   if(*off >= MAX_STORAGE_SIZE) {//offset too large
       mutex_unlock(&(the_object->operation_synchronizer));
       return -ENOSPC;//no space left on device
   }
   if(*off > the_object->valid_bytes) {//offset bwyond the current stream size
       mutex_unlock(&(the_object->operation_synchronizer));
       return -ENOSR;//out of stream resources
   }
   if((MAX_STORAGE_SIZE - *off) < len) len = MAX_STORAGE_SIZE - *off;
   ret = copy_from_user(&(the_object->stream_content[*off]),buff,len);

   *off += (len - ret);
   the_object->valid_bytes = *off;
   mutex_unlock(&(the_object->operation_synchronizer));

   return len - ret;*/
    return ret;

}

static ssize_t dev_read(struct file *file, char *buff, size_t len, loff_t *off) {

    int minor = get_minor(file);
    int ret=0;
    DEBUG
        printk("%s: somebody called a READ on dev with minor number %d\n", MODNAME, minor);

    /*   object_state *the_object;

   the_object = device_instance + minor;
   printk("%s: somebody called a read on dev with [major,minor] number [%d,%d]\n",MODNAME,get_major(filp),get_minor(filp));

   //need to lock in any case
    mutex_lock(&(the_object->operation_synchronizer));
    if(*off > the_object->valid_bytes) {
        mutex_unlock(&(the_object->operation_synchronizer));
        return 0;
    }
    if((the_object->valid_bytes - *off) < len) len = the_object->valid_bytes - *off;
    ret = copy_to_user(buff,&(the_object->stream_content[*off]),len);

    *off += (len - ret);
    mutex_unlock(&(the_object->operation_synchronizer));

    return len - ret;*/
    return ret;

}

static long dev_ioctl(struct file *file, unsigned int command, unsigned long param) {

    /*   int minor = get_minor(file);
       object_state *the_object;

       the_object = device_instance + minor;*/

    single_session* session;
    session = file->private_data;

    printk("%s: ioctl() has been called on dev with (command: %u, value: %lu)\n", MODNAME, command, param);

    switch (command){
        case SET_SEND_TIMEOUT:
            session->write_timer = param;
            break;
        case SET_RECV_TIMEOUT:
            session->read_timer = param;
            break;
        case REVOKE_DELAYED_MESSAGES:
            break;
        default:
            printk(KERN_ERR "%s: ioctl() has been called with unexpected command: %u\n", MODNAME, command);
            return -EINVAL; /* Invalid argument */
    }
    return 0;
}

static int dev_flush(struct file *file, fl_owner_t owner){
    int minor = get_minor(file);
    int ret=0;

    DEBUG
        printk("%s: somebody called a FLUSH on dev with minor number %d\n", MODNAME, minor);
    return ret;
}

static struct file_operations fops = {
        .owner = THIS_MODULE,
        .open = dev_open,
        .release = dev_release,
        .read = dev_read,
        .write = dev_write,
        .unlocked_ioctl = dev_ioctl,
        .flush = dev_flush,
};



static int __init add_dev(void) {

    int i;

    //initialize the drive internal state -> GLOBAL "FILE" variables
    for(i=0; i<MINORS; i++){
        /*TODO: delete quaglia not using
        mutex_init(&(device_instance[i].object_busy));
        mutex_init(&(device_instance[i].operation_synchronizer));
        device_instance[i].valid_bytes = 0;
        device_instance[i].stream_content = NULL;
        device_instance[i].stream_content = (char*)__get_free_page(GFP_KERNEL);
        if(device_instance[i].stream_content == NULL) goto revert_allocation;*/

        //init delle strutture dei dati puri
        instance_by_minor[i].actual_total_size = 0;
        INIT_LIST_HEAD(&instance_by_minor[i].stored_messages);

        //lista delle sessioni multiple per singolo minor
        INIT_LIST_HEAD(&instance_by_minor[i].all_sessions);

        //others
        mutex_init(&(instance_by_minor->write_mutex));
        mutex_init(&(instance_by_minor->read_mutex));

    }

    Major = __register_chrdev(0, 0, MINORS, DEVICE_NAME, &fops);
    //actually allowed minors are directly controlled within this driver

    if (Major < 0) {
        printk(KERN_ERR "%s: registering device failed\n",MODNAME);
        return Major;
    }

    printk(KERN_INFO "%s: new device registered, it is assigned major number %d\n", MODNAME, Major);
    DEBUG
        printk(KERN_DEBUG "%s: init della coda all'indirizzo %p\n", MODNAME, &instance_by_minor[0].all_sessions);


    return 0;

    /* revert_allocation:
     for(;i>=0;i--){
         free_page((unsigned long)device_instance[i].stream_content);
     }
     return -ENOMEM;*/
}

static void __exit remove_dev(void){

    int i;
    for(i=0; i<MINORS; i++){
        //kfree((unsigned long)instance_by_minor[i]);
    }

    unregister_chrdev(Major, DEVICE_NAME);

    printk(KERN_INFO "%s: new device unregistered, it was assigned major number %d\n",MODNAME, Major);

}

MODULE_LICENSE("GPL");
MODULE_AUTHOR("dilettalagom");
MODULE_DESCRIPTION("CoreMess module.");
MODULE_VERSION("0.01");

module_init(add_dev)
module_exit(remove_dev)
