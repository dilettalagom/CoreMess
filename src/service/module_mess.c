#include "module_mess.h"


static unsigned long max_message_size = MAX_MESSAGE_SIZE;
module_param(max_message_size, ulong, PERM_MODE);

static unsigned long max_storage_size = MAX_STORAGE_SIZE;
module_param(max_storage_size, ulong, PERM_MODE);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 0, 0)
#define get_major(file)	MAJOR(file->f_inode->i_rdev)
#define get_minor(file)	MINOR(file->f_inode->i_rdev)
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
    printk("%s: start opening object with minor %d\n", MODNAME, minor);

    //Init single_session metadata
    session = kmalloc(sizeof(single_session), GFP_KERNEL);
    if(session == NULL){
        printk(KERN_ERR "%s: kmalloc() failed in dev_open\n", MODNAME);
        return -ENOMEM; /* Out of memory */
    }
    DEBUG
        printk("%s: kmalloc succeded\n", MODNAME);

    session->write_timer = ktime_set(0, 0);
    session->read_timer = ktime_set(0, 0);
    INIT_LIST_HEAD(&(session->next));

    //TODO:hrtimer_init(&(session->read_timer), CLOCK_MONOTONIC, HRTIMER_MODE_REL);


    file->private_data = (void *) session;

    //add new session for same minor in specific minor_sessions_linked_list
    mutex_lock(&(instance_by_minor[minor].dev_mutex));
    list_add_tail(&(session->next), &(instance_by_minor[minor].all_sessions));
    mutex_unlock(&(instance_by_minor[minor].dev_mutex));

    //device opened by a default nop
    printk("%s: device file successfully opened for object with minor %d\n", MODNAME, minor);
    return 0;


}


static int dev_release(struct inode *inode, struct file *file) {

    int minor;
    single_session* session;

    minor = get_minor(file);
    DEBUG
        printk("%s: RELEASE has been called on %d minor\n", MODNAME, minor);

    session = (single_session*) file->private_data;

    mutex_lock(&(instance_by_minor[minor].dev_mutex));
    list_del(&session->next);
    mutex_unlock(&(instance_by_minor[minor].dev_mutex));

    kfree(session);

    //device closed by default nop
    printk("%s: Device file closed\n",MODNAME);
    return 0;

}

static ssize_t dev_write(struct file *file, const char *buff, size_t len, loff_t *off) {

    int minor;
    single_session* session;
    message_t* new_message;


    minor = get_minor(file);
    DEBUG
        printk("%s: somebody called a WRITE on dev with minor number %d\n", MODNAME, minor);

    //check for the new message size
    if (len > max_message_size) {
        DEBUG
            printk(KERN_ERR "%s: the new message is bigger than the max_message_size\n",MODNAME);
        return -EMSGSIZE;
    }

    //alloc space for the new message
    new_message = kmalloc(sizeof(message_t), GFP_KERNEL);
    if (new_message == NULL) {
        DEBUG
            printk(KERN_ERR "%s: kalloc(message_t) failed in dev_write\n",MODNAME);
        return -ENOMEM;
    }
    INIT_LIST_HEAD(&(new_message->next));

    new_message->text = kmalloc(len, GFP_KERNEL);
    if (new_message->text == NULL) {
        DEBUG
            printk(KERN_ERR "%s: kalloc(text) failed in dev_write\n",MODNAME);
        return -ENOMEM;
    }

    session = (single_session*)file->private_data;

    if (session->write_timer) { //DEFERRED_WRITE
        //TODO
        DEBUG
            printk("%s: a DEFERRED WRITE has been invoked\n", MODNAME);


    }else { //NO_WAIT_WRITE
        DEBUG
            printk("%s: a NO_WAIT_WRITE has been invoked\n", MODNAME);
        /*if(*off > the_object->valid_bytes) {//offset bwyond the current stream size
            mutex_unlock(&(instance_by_minor[minor].write_mutex));
            return -ENOSR;//out of stream resources
        }*/

        mutex_lock(&(instance_by_minor[minor].dev_mutex));
        //check if the message can be stored in the device
        if(instance_by_minor[minor].actual_total_size + len >= max_storage_size) {
            mutex_unlock(&(instance_by_minor[minor].dev_mutex));
            return -ENOSPC; /* no space left on device */
        }

        //add messagge to device structure
        if (copy_from_user(new_message->text, buff, len)){
            DEBUG
                printk(KERN_ERR "%s: copy_from_user in write failed\n",MODNAME);
            return -EMSGSIZE;
        }
        printk("%s: copy_from_user in write: %s\n",MODNAME, new_message->text);
//        list_add_tail(&(new_message->next), &(instance_by_minor[minor].stored_messages));
//
//        instance_by_minor[minor].actual_total_size += len;
//        DEBUG
//            printk("%s write(), new size: %u\n",MODNAME,  instance_by_minor[minor].actual_total_size);
//        mutex_unlock(&(instance_by_minor[minor].dev_mutex));

    }
    return 0;

}

static ssize_t dev_read(struct file *file, char *buff, size_t len, loff_t *off) {

    int minor;
    single_session* session;
    message_t* head_message;
    unsigned int head_m_size;

    minor = get_minor(file);
    DEBUG
        printk("%s: somebody called a READ on dev with minor number %d\n", MODNAME, minor);

    //need to lock in any case
    mutex_lock(&(instance_by_minor[minor].dev_mutex));
    head_message = list_first_entry(&(instance_by_minor[minor].stored_messages), message_t, next);

    session = (single_session*)file->private_data;

    if (session->read_timer) { //DEFERRED_READ
        //TODO
        DEBUG
            printk("%s: a DEFERRED READ has been invoked\n", MODNAME);

    }else { //NO_WAIT_READ
        head_m_size = strlen(head_message->text);
        DEBUG
            printk("%s: a NO_WAIT_READ has been invoked\n", MODNAME);

        //add messagge to device structure
        if (copy_to_user(buff, &(head_message->text), head_m_size)){
            DEBUG
                printk(KERN_ERR "%s: copy_to_user in read failed\n",MODNAME);
            return -EMSGSIZE;
        }

        //list_del(&(head_message->next));
        instance_by_minor[minor].actual_total_size -= head_m_size;
        mutex_unlock(&(instance_by_minor[minor].dev_mutex));
        //kfree(head_message->text);
        //kfree(head_message);
    }
    return 0;

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

}

static long dev_ioctl(struct file *file, unsigned int command, unsigned long param) {

    single_session* session;
    session = file->private_data;

    printk("%s: ioctl() has been called on dev with (command: %u, value: %lu)\n", MODNAME, command, param);

    switch (command){
        case SET_SEND_TIMEOUT: //0
            session->write_timer = ktime_set(0, param*1000); //param : microsecs*1000=ns
            DEBUG
                printk("%s: ioctl() has set SET_SEND_TIMEOUT:%llu\n", MODNAME, session->write_timer);
            break;
        case SET_RECV_TIMEOUT: //1
            session->read_timer = ktime_set(0, param*1000);
            DEBUG
                printk("%s: ioctl() has set SET_RECV_TIMEOUT:%llu\n", MODNAME, session->read_timer);
            break;
        case REVOKE_DELAYED_MESSAGES: //2
            break;
        default:
            printk(KERN_ERR "%s: ioctl() has been called with unexpected command: %u\n", MODNAME, command);
            return -EINVAL; /* Invalid argument */
    }
    return 0;
}

static int dev_flush(struct file *file, fl_owner_t owner){
    int minor;
    //struct list_head *ptr;
    //single_session* session;

    minor = get_minor(file);
    mutex_lock(&(instance_by_minor[minor].dev_mutex));

    /*TODO:
     * list_for_each(ptr, &(instance_by_minor[minor].all_sessions)) {
        session = list_entry(ptr, single_session, next);
        list_del(&session->next);
        kfree(session);
    }*/
    mutex_unlock(&(instance_by_minor[minor].dev_mutex));


    DEBUG
        printk("%s: somebody called a FLUSH on dev with minor number %d\n", MODNAME, minor);
    return 0;
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

        //init delle strutture dei dati puri
        instance_by_minor[i].actual_total_size = 0;
        INIT_LIST_HEAD(&instance_by_minor[i].stored_messages);

        //lista delle sessioni multiple per singolo minor
        INIT_LIST_HEAD(&instance_by_minor[i].all_sessions);

        //per rendere l'accesso alle strutture globali univoco
        mutex_init(&(instance_by_minor->dev_mutex));

        //TODO:deferred structs
    }

    Major = __register_chrdev(0, 0, MINORS, DEVICE_NAME, &fops);
    if (Major < 0) {
        printk(KERN_ERR "%s: registering device failed\n",MODNAME);
        return Major;
    }

    printk(KERN_INFO "%s: New device registered, it is assigned major number %d\n", MODNAME, Major);
    DEBUG
        printk(KERN_DEBUG "%s: init della coda all'indirizzo %p\n", MODNAME, &instance_by_minor[0].all_sessions);
    return 0;

}

static void __exit remove_dev(void){

    int i;
    struct list_head *ptr;
    struct list_head *tmp;
    message_t *message;

    for(i=0; i<MINORS; i++){
        list_for_each_safe(ptr, tmp, &(instance_by_minor[i].stored_messages)) {
            message = list_entry(ptr, message_t, next);
            list_del(&(message->next));
            kfree(message->text);
            kfree(message);
        }
    }

    unregister_chrdev(Major, DEVICE_NAME);
    printk(KERN_INFO "%s: New device unregistered, it was assigned major number %d\n",MODNAME, Major);
    return;
}

MODULE_LICENSE("GPL");
MODULE_AUTHOR("dilettalagom");
MODULE_DESCRIPTION("CoreMess module.");
MODULE_VERSION("0.01");

module_init(add_dev)
module_exit(remove_dev)
