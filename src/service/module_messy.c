#include "module_messy.h"


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

    mutex_init(&session->operation_mutex);
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

static int __add_new_message(device_instance* instance, char* temp, size_t len){

    message_t* new_message;

    //check if message can be stored
    if (instance->actual_total_size + len > max_storage_size) {
        kfree(temp);
        printk(KERN_ERR "%s: OOM: this device is full!\n",MODNAME);
        return -ENOSPC;
    }

    //create new message
    new_message = kmalloc(sizeof(message_t), GFP_KERNEL);
    if (new_message == NULL) {
        kfree(temp);
        printk(KERN_ERR "%s: kmalloc() failed in __add_new_message()\n", MODNAME);
        return -ENOMEM;
    }
    new_message->text = temp;
    INIT_LIST_HEAD(&(new_message->next));

    //update global device state
    list_add_tail(&(new_message->next), &(instance->stored_messages));
    instance->actual_total_size += len;

    return len;
}

static ssize_t dev_write(struct file *file, const char *buff, size_t len, loff_t *off) {

    int minor, ret = 0;
    char* temp;
    single_session* session;
    struct list_head *head, *pos;

    minor = get_minor(file);
    DEBUG
        printk("%s: somebody called a WRITE on dev with minor number %d\n", MODNAME, minor);

    //check for the new message size
    if (len > max_message_size) {
        DEBUG
            printk(KERN_ERR "%s: the new message is bigger than the max_message_size\n",MODNAME);
        return -EMSGSIZE;
    }

    //get message from user-level (serve un puntatore nuovo ogni volta, con il buffer non funziona. dho!)
    temp = kmalloc(len,GFP_KERNEL);
    if (temp == NULL) {
        DEBUG
            printk(KERN_ERR "%s: kmalloc() failed in dev_write()\n", MODNAME);
        return -ENOMEM;
    }
    ret = copy_from_user(temp, buff, len);

    if (ret) {
        kfree(temp);
        DEBUG
            printk(KERN_ERR "%s: copy_from_user in write failed\n",MODNAME);
        return -EMSGSIZE;
    }

    session = (single_session*)file->private_data;

    /*TODO: prima di accedere al write_timer dovrei lockare perchè qualcuno potrebbe accedervi da sys mentre sto per usarl
     * e cambiarlo. Meglio se lo copio in un valore qui locale*/
    if (session->write_timer) { //DEFERRED_WRITE
        //TODO
        DEBUG
            printk("%s: a DEFERRED WRITE has been invoked\n", MODNAME);

    }else { //NO_WAIT_WRITE

        DEBUG
            printk("%s: a NO_WAIT_WRITE has been invoked\n", MODNAME);

        mutex_lock(&(instance_by_minor[minor].dev_mutex));
        ret = __add_new_message(&instance_by_minor[minor], temp, len);
        DEBUG
            printk("%s write(), new size: %lu\n",MODNAME, instance_by_minor[minor].actual_total_size);


        //TODO:cancella le righe sotto-> solo test

        if(!list_empty(&(instance_by_minor[minor].stored_messages))) {

            printk("%s: instance_by_minor[minor].stored_messages not empty: printing...\n", MODNAME);
            list_for_each_safe(head, pos, &(instance_by_minor[minor].stored_messages)){
                message_t *elt;
                elt = list_first_entry(pos, message_t, next);
                printk("%s write(), messaggio aggiunto nella lista: %s\n",MODNAME, elt->text);
            }
        }

        mutex_unlock(&(instance_by_minor[minor].dev_mutex));

    }
    // Most write functions return the number of bytes put into the buffer
    return ret;

}


static int __send_first_message(device_instance* instance, char* buff, ssize_t len){

    int ret = 0;
    message_t* head_message;
    unsigned int head_message_len;

    //get first message_t safely
    if(!list_empty(&(instance->stored_messages))) {
        head_message = list_first_entry(&(instance->stored_messages), message_t, next);
        head_message_len = strlen(head_message->text) + 1;

        DEBUG
            printk( "%s: __send_first_message, head_message->text:%s, size:%u\n",MODNAME, head_message->text, head_message_len);

        //user can request less bytes than the ones stored...
        if(head_message_len > len)
            head_message_len = len;

        //send message->text to user
        ret = copy_to_user(buff, head_message->text, head_message_len);
        if(ret){
            DEBUG
                printk(KERN_ERR "%s: copy_to_user in __send_first_message failed\n",MODNAME);
            return -EMSGSIZE;
        }

        //update global device state
        list_del(&head_message->next);
        instance->actual_total_size -= strlen(head_message->text); //...but the device will be reduced by the exact size anyway

        //dealloc head of instance->stored_messages
        kfree(head_message->text);
        kfree(head_message);

        return (int)head_message_len - ret;
    }

    DEBUG
        printk("%s: nothing to read in instance_by_minor[minor].stored_messages\n",MODNAME);
    return ret;
}


static ssize_t dev_read(struct file *file, char *buff, size_t len, loff_t *off) {

    int minor, ret = 0;
    single_session* session;

    minor = get_minor(file);
    DEBUG
        printk("%s: somebody called a READ on dev with minor number %d\n", MODNAME, minor);

    //check for the requested message size
    if (len > max_message_size) {
        DEBUG
            printk(KERN_ERR "%s: the requested message is bigger than the max_message_size\n",MODNAME);
        return -EMSGSIZE;
    }

    session = (single_session*)file->private_data;

    if(session->read_timer){//DEFERRED_READ
        //TODO
        DEBUG
            printk("%s: a DEFERRED READ has been invoked\n", MODNAME);
    }else {
        DEBUG
            printk("%s: a NO_WAIT_READ has been invoked\n", MODNAME);

        mutex_lock(&(instance_by_minor[minor].dev_mutex));
        ret = __send_first_message(&instance_by_minor[minor], buff, len);
        DEBUG
            printk("%s read(), new size: %lu\n",MODNAME, instance_by_minor[minor].actual_total_size);
        mutex_unlock(&(instance_by_minor[minor].dev_mutex));

    }

    // Most read functions return the number of bytes put into the buffer
    return ret;



}

static void __del_all_messages(int minor){
    struct list_head *ptr, *q;
    message_t* message;

    if(!list_empty(&(instance_by_minor[minor].stored_messages))) {
        DEBUG
            printk("%s: instance_by_minor[minor].stored_messages not empty: deleting...\n", MODNAME);
        list_for_each_safe(ptr, q, &(instance_by_minor[minor].stored_messages)) {
            message = list_entry(ptr, message_t, next);
            list_del(&message->next);
            kfree(message->text);
            kfree(message);
        }
    }
    instance_by_minor[minor].actual_total_size = 0;

    /*TODO:delete stored_messages IF NEEDED
     *
     *DEBUG
     *printk("%s: instance_by_minor[minor].stored_messages is empty: deleting struct...\n", MODNAME);
    *list_del(&(instance_by_minor[minor].stored_messages));
    *DEBUG
    *   printk("%s: all messages in instance_by_minor[minor].stored_messages have been deleted\n", MODNAME);
    */
}


static long dev_ioctl(struct file *file, unsigned int command, unsigned long param) {
    int minor;
    single_session* session;

    minor = get_minor(file);
    session = file->private_data;

    printk("%s: ioctl() has been called on dev with (command: %u, value: %lu)\n", MODNAME, command, param);

    switch (command){
        case SET_SEND_TIMEOUT: //27392
            mutex_lock(&session->operation_mutex);
            session->write_timer = ktime_set(0, param*1000); //param : microsecs*1000=ns
            DEBUG
                printk("%s: ioctl() has set SET_SEND_TIMEOUT:%llu\n", MODNAME, session->write_timer);
            mutex_unlock(&session->operation_mutex);
            break;
        case SET_RECV_TIMEOUT: //27393 ?
            mutex_lock(&session->operation_mutex);
            session->read_timer = ktime_set(0, param*1000);
            DEBUG
                printk("%s: ioctl() has set SET_RECV_TIMEOUT:%llu\n", MODNAME, session->read_timer);
            mutex_unlock(&session->operation_mutex);
            break;
        case REVOKE_DELAYED_MESSAGES: //27394
            DEBUG
                printk("%s: ioctl() has called REVOKE_DELAYED_MESSAGES\n", MODNAME);
            mutex_lock(&instance_by_minor[minor].dev_mutex);
            __del_all_messages(minor);
            mutex_unlock(&instance_by_minor[minor].dev_mutex);
            break;
        default:
            printk(KERN_ERR "%s: ioctl() has been called with unexpected command: %u\n", MODNAME, command);
            return -EINVAL; /* Invalid argument */
    }
    return 0;
}



static int dev_flush(struct file *file, fl_owner_t owner){
    int minor;

    minor = get_minor(file);
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

static void __del_all_sessions(device_instance* instance) {
    struct list_head *ptr;
    single_session* session;
    int i;

    for (i = 0; i < MINORS; i++) {
        list_for_each(ptr, &(instance[i].all_sessions)) {
            session = list_entry(ptr, single_session, next);
            list_del(&session->next);
            kfree(session);
        }
    }
    DEBUG
        printk("%s: all sessions in instance_by_minor[minor].all_sessions have been deleted\n", MODNAME);
}

static void __exit remove_dev(void){

    /*deleting all messagges
    mutex_lock(&(instance_by_minor[minor].dev_mutex));
    __del_all_messages(minor);
    mutex_unlock(&(instance_by_minor[minor].dev_mutex));*/

    //deleting all sessions
    __del_all_sessions((instance_by_minor));

    unregister_chrdev(Major, DEVICE_NAME);
    printk(KERN_INFO "%s: New device unregistered, it was assigned major number %d\n",MODNAME, Major);
    return;
}

MODULE_LICENSE("GPL");
MODULE_AUTHOR("dilettalagom");
MODULE_DESCRIPTION("CoreMess module.");
//MODULE_VERSION("0.01");

module_init(add_dev)
module_exit(remove_dev)
