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
device_instance instance_by_minor[MINORS];


//the actual driver
static int dev_open(struct inode *inode, struct file *file) {

    single_session* session;
    int minor = get_minor(file);

    if(minor >= MINORS){
        printk(KERN_ERR "%s: Wrong minor number %d\n", MODNAME, minor);
        return -ENODEV;
    }
    printk("%s: start opening object with minor %d\n", MODNAME, minor);

    //init single_session metadata
    session = kmalloc(sizeof(single_session), GFP_KERNEL);
    if(session == NULL){
        printk(KERN_ERR "%s: kmalloc() failed in dev_open()\n", MODNAME);
        return -ENOMEM; /* Out of memory */
    }

    mutex_init(&session->operation_mutex);
    session->write_timer = 0;
    session->read_timer = ktime_set(0, 0);

    //deferred struct_per_session
    INIT_LIST_HEAD(&session->pending_defwrite_structs);
    session->pending_write_wq = alloc_workqueue("coremesswq", WQ_MEM_RECLAIM, 0);
    if (session->pending_write_wq == NULL) {
        kfree(session);
        return -ENOMEM;
    }

    //add session's data to fd
    file->private_data = (void *) session;

    //add new session for same minor in specific minor_sessions_linked_list
    mutex_lock(&instance_by_minor[minor].dev_mutex);
    list_add_tail(&session->next, &instance_by_minor[minor].all_sessions);
    mutex_unlock(&instance_by_minor[minor].dev_mutex);

    //device opened by a default nop
    printk(KERN_INFO "%s: Device file successfully opened for object with minor %d\n", MODNAME, minor);
    return 0;

}

static int dev_release(struct inode *inode, struct file *file) {

    int minor = get_minor(file);
    single_session* session = file->private_data;

    DEBUG
        printk("%s: RELEASE has been called on %d minor\n", MODNAME, minor);


    //deleting session's empty structs
    destroy_workqueue(session->pending_write_wq);
    list_del(&session->pending_defwrite_structs);

    //delete current session pointer
    mutex_lock(&instance_by_minor[minor].dev_mutex);
    list_del(&session->next);
    mutex_unlock(&instance_by_minor[minor].dev_mutex);

    kfree(session);

    //device closed by default nop
    printk(KERN_INFO "%s: Device file closed\n",MODNAME);
    return 0;

}


static int __add_new_message(device_instance* instance, message_t* new_message){

    DEBUG
        printk("%s: __add_new_message %s", MODNAME, new_message->text);

    //check if message can be stored
    if (instance->actual_total_size + new_message->len > max_storage_size) {
        kfree(new_message->text);
        kfree(new_message);
        printk(KERN_ERR "%s: OOM: this device is full!\n",MODNAME);
        return -ENOSPC;
    }

    //update global device state
    mutex_lock(&instance->dev_mutex);
    list_add_tail(&new_message->next, &instance->stored_messages);
    instance->actual_total_size += new_message->len;
    __atomic_fetch_add(&instance->num_pending_read, 1, __ATOMIC_SEQ_CST);

    //notify to the readers that a new message is ready
    wake_up(&instance->deferred_read);
    mutex_unlock(&instance->dev_mutex);

    return new_message->len;
}


static void __deferred_add_new_message(struct work_struct *w){

    int ret;
    struct delayed_work *delayed_work;
    pending_write_t* pending_write;

    delayed_work = container_of(w, struct delayed_work, work);
    pending_write = container_of(delayed_work, pending_write_t, the_deferred_write_work);

    printk("%s: pending mesg: %s\n", MODNAME, pending_write->pending_message->text);

    ret = __add_new_message(&instance_by_minor[pending_write->minor], pending_write->pending_message);
    if(ret)
        DEBUG
            printk("%s: return from __deferred_write", MODNAME);
}


static ssize_t dev_write(struct file *file, const char *buff, size_t len, loff_t *off) {

    int minor, ret = 0;
    unsigned long write_timer;
    message_t* new_message;
    single_session* session;
    pending_write_t* pending_write;

    minor = get_minor(file);
    DEBUG
        printk("%s: someone called a WRITE on dev with minor number %d\n", MODNAME, minor);

    //check for the new message size, eventually resize
    if (len > max_message_size) {
        len = max_message_size;
    }

    //creating the new message in kernel side
    new_message = kmalloc(sizeof(message_t),GFP_KERNEL);
    if(new_message == NULL){
        DEBUG
            printk(KERN_ERR "%s: kmalloc() failed in dev_write()\n", MODNAME);
        return -ENOMEM;
    }
    new_message->text = kmalloc(len, GFP_KERNEL);
    if(new_message->text == NULL){
        DEBUG
            printk(KERN_ERR "%s: kmalloc() failed in dev_write()\n", MODNAME);
        return -ENOMEM;
    }
    ret = copy_from_user(new_message->text, buff, len);
    if (ret) {
        kfree(new_message->text);
        kfree(new_message);
        DEBUG
            printk(KERN_ERR "%s: copy_from_user() in dev_write() failed\n",MODNAME);
        return -EMSGSIZE; /* Message too long */
    }
    new_message->len = len;

    session = (single_session*)file->private_data;

    //get the write_timer value before someone else changes it
    mutex_lock(&session->operation_mutex);
    write_timer = session->write_timer;
    mutex_unlock(&session->operation_mutex);

    if (write_timer) { //DEFERRED_WRITE

        DEBUG
            printk("%s: a DEFERRED_WRITE has been invoked\n", MODNAME);

        //create pending_write struct (initializes a work_struct item)
        pending_write = kmalloc(sizeof(pending_write_t), GFP_KERNEL); /* GFP_ATOMIC -> non blocking memory allocation */
        if(pending_write == NULL) {
            DEBUG
                printk(KERN_ERR "%s: kmalloc() failed in dev_write()\n", MODNAME);
            return -ENOMEM;
        }
        pending_write->minor = minor;
        pending_write->pending_message = new_message;

        //init deferred write work
        INIT_DELAYED_WORK(&(pending_write->the_deferred_write_work), (void*) __deferred_add_new_message);

        mutex_lock(&session->operation_mutex);
        //update global device state about pending write
        list_add_tail(&pending_write->next, &session->pending_defwrite_structs);

        //queueing a new deferred write
        queue_delayed_work(session->pending_write_wq, //work_queue
                           &pending_write->the_deferred_write_work, //new task code
                           write_timer); //time_delay in jiffies
        mutex_unlock(&session->operation_mutex);

    }else { //NO_WAIT_WRITE

        DEBUG
            printk("%s: a NO_WAIT_WRITE has been invoked\n", MODNAME);

        ret = __add_new_message(&instance_by_minor[minor], new_message);
        DEBUG
            printk("%s write(), new size: %lu\n",MODNAME, instance_by_minor[minor].actual_total_size);
    }

    //Most write functions return the number of bytes put into the buffer
    return ret;

}


static int __send_first_message(device_instance* instance, char* buff, ssize_t len){

    int ret = 0;
    message_t* head_message;
    unsigned int head_message_len;

    //get first message_t safely
    if(!list_empty(&(instance->stored_messages))) {

        mutex_lock(&instance->dev_mutex);

        head_message = list_first_entry(&(instance->stored_messages), message_t, next);
        head_message_len = head_message->len;

        DEBUG
            printk( "%s: __send_first_message, head_message->text:%s, size:%u\n",MODNAME, head_message->text, head_message_len);

        //user can request less bytes than the ones stored...
        if(head_message_len > len)
            head_message_len = len;

        //send message->text to user
        ret = copy_to_user(buff, head_message->text, head_message_len);
        if(ret){
            DEBUG
                printk(KERN_ERR "%s: copy_to_user() in __send_first_message failed\n",MODNAME);
            return -EMSGSIZE; /* Message too long */
        }

        //update global device state
        list_del(&head_message->next);
        instance->actual_total_size -= head_message->len; //...but the device will be reduced by the exact size anyway
        __atomic_fetch_sub(&instance->num_pending_read,  1, __ATOMIC_SEQ_CST);

        //dealloc head of instance->stored_messages
        kfree(head_message->text);
        kfree(head_message);

        mutex_unlock(&instance->dev_mutex);

        DEBUG
            printk("%s: __send_first_message(): message send to user\n",MODNAME);
        return (int)head_message_len - ret;
    }

    DEBUG
        printk(KERN_ERR "%s: __send_first_message(): should NOT BE here\n",MODNAME);
    return ret;
}


static ssize_t dev_read(struct file *file, char *buff, size_t len, loff_t *off) {

    int minor, back_from_sleep, ret = 0;
    single_session* session;
    ktime_t read_timer;
    read_subscription_t* r_subscription;


    minor = get_minor(file);
    DEBUG
        printk("%s: someone called a READ on dev with minor number %d\n", MODNAME, minor);

    session = (single_session*)file->private_data;

    //get the read_timer value before someone else changes it
    mutex_lock(&session->operation_mutex);
    read_timer = session->read_timer;
    mutex_unlock(&session->operation_mutex);

    if(read_timer){//DEFERRED_READ
        DEBUG
            printk("%s: a DEFERRED READ has been invoked\n", MODNAME);

        //create a reader_subscription for the flush event
        r_subscription = kmalloc(sizeof(read_subscription_t),GFP_KERNEL);
        if(r_subscription == NULL){
            DEBUG
                printk(KERN_ERR "%s: kmalloc() failed in DEFERRED_READ\n", MODNAME);
            return -ENOMEM;
        }
        r_subscription->flush_me = false;

        mutex_lock(&instance_by_minor[minor].dev_mutex);
        list_add(&r_subscription->next, &instance_by_minor[minor].readers_subscriptions);
        mutex_unlock(&instance_by_minor[minor].dev_mutex);

        //add the deferred_reader to the wait_queue
        back_from_sleep = wait_event_interruptible_hrtimeout(instance_by_minor[minor].deferred_read, //wait_queue
                                                             instance_by_minor[minor].num_pending_read > 0  || r_subscription->flush_me == true, //condition to sleep on
                                                             read_timer); //timer
        switch (back_from_sleep){
            case 0:
                //flush all readers
                if(r_subscription->flush_me == true){
                    ret = -EINTR; /* Interrupted system call */
                }else {
                    //new message available to read
                    ret = __send_first_message(&instance_by_minor[minor], buff, len);
                }
                //removing reader_subscription from the device
                list_del(&r_subscription->next);
                kfree(r_subscription);
                return ret;
            case -ETIME:
                //timeout expired: nothing to read
                DEBUG
                    printk("%s: timeout expired -> no messages\n", MODNAME);
                return -ETIME;
            case -ERESTARTSYS:
                //interrupted
                DEBUG
                    printk("%s: ERESTARTSYS -> interrupted by a signal\n", MODNAME);
                return -ERESTART;
            default:
                printk(KERN_ERR "%s: read() has returned with unexpected ret: %d\n", MODNAME, back_from_sleep);
                return -EINVAL; /* Invalid argument */
        }
    }else if(instance_by_minor[minor].num_pending_read > 0 ) { //NO_WAIT READ

        DEBUG printk("%s: a NO_WAIT_READ has been invoked\n", MODNAME);

        ret = __send_first_message(&instance_by_minor[minor], buff, len);
        DEBUG printk("%s read(), new size: %lu\n", MODNAME, instance_by_minor[minor].actual_total_size);
    }

    //Most read functions return the number of bytes put into the buffer
    return ret;
}


static void __del_session_defread(int minor) {
    struct list_head *ptr;
    read_subscription_t* r_sub;

    if(!list_empty(&instance_by_minor[minor].readers_subscriptions)){
        DEBUG
            printk("%s: readers_subscriptions not empty: deleting...\n", MODNAME);
        list_for_each(ptr, &instance_by_minor[minor].readers_subscriptions) {
            r_sub = list_entry(ptr, read_subscription_t, next);
            //changing wake-up condition for flush event
            r_sub->flush_me = true;
        }
    }
    wake_up(&instance_by_minor[minor].deferred_read);
    DEBUG
        printk("%s: readers_subscriptions is empty\n", MODNAME);
}


static void __del_session_defwrite(single_session* session) {
    struct list_head *ptr, *tmp;
    pending_write_t* pending_write;

    if(!list_empty(&session->pending_defwrite_structs)) {
        DEBUG
            printk("%s: pending_defwrite_structs not empty: deleting...\n", MODNAME);

        list_for_each_safe(ptr, tmp, &session->pending_defwrite_structs) {
            pending_write = list_entry(ptr, pending_write_t, next);

            if (cancel_delayed_work(&pending_write->the_deferred_write_work)){
                //del pending message only if the work has been deleted already
                DEBUG
                    printk("%s: work was pending\n", MODNAME);
                kfree(pending_write->pending_message->text);
                kfree(pending_write->pending_message);

                //del pending struct
                list_del(&pending_write->next);
                kfree(pending_write);
            }
        }
    }
    DEBUG
        printk("%s: pending_defwrite_structs is empty\n", MODNAME);
}


static void __del_all_deferred_writes(int minor){

    struct list_head *ptr;
    single_session* session;

    if(!list_empty(&instance_by_minor[minor].all_sessions)) {
        list_for_each(ptr, &(instance_by_minor[minor].all_sessions)) {
            session = list_entry(ptr, single_session, next);
            mutex_lock(&session->operation_mutex);
            __del_session_defwrite(session);
            flush_workqueue(session->pending_write_wq);
            mutex_unlock(&session->operation_mutex);
        }
    }
}


static void __del_all_deferred_works(int minor){
    struct list_head *ptr;
    single_session* session;

    DEBUG
        printk("%s: i'm in __del_all_deferred_works\n", MODNAME);

    if(!list_empty(&instance_by_minor[minor].all_sessions)) {
        //wake up all readers
        __del_session_defread(minor);

        //remove all pending writes
        list_for_each(ptr, &instance_by_minor[minor].all_sessions) {
            session = list_entry(ptr, single_session, next);
            mutex_lock(&session->operation_mutex);
            __del_session_defwrite(session);
            flush_workqueue(session->pending_write_wq);
            mutex_unlock(&session->operation_mutex);
        }
    }
}


static void __del_all_messages(int minor){
    struct list_head *ptr, *q;
    message_t* message;

    if(!list_empty(&instance_by_minor[minor].stored_messages)) {
        DEBUG
            printk("%s: stored_messages not empty: deleting...\n", MODNAME);
        list_for_each_safe(ptr, q, &instance_by_minor[minor].stored_messages) {
            message = list_entry(ptr, message_t, next);
            list_del(&message->next);
            kfree(message->text);
            kfree(message);
        }
    }
    instance_by_minor[minor].actual_total_size = 0;
    instance_by_minor[minor].num_pending_read = 0;
}


static long dev_ioctl(struct file *file, unsigned int command, unsigned long param) {

    int minor = get_minor(file);
    single_session* session = file->private_data;

    printk("%s: ioctl() has been called on dev with (command: %u, value: %lu)\n", MODNAME, command, param);

    switch (command){
        case SET_SEND_TIMEOUT: //27392
            mutex_lock(&session->operation_mutex);
            session->write_timer = ((unsigned long) param * HZ) /1000; /* HZ = 1 second in jiffies -> param in milliseconds */
            DEBUG
                printk("%s: ioctl() has set SET_SEND_TIMEOUT:%lu\n", MODNAME, session->write_timer);
            mutex_unlock(&session->operation_mutex);
            break;
        case SET_RECV_TIMEOUT: //27393
            mutex_lock(&session->operation_mutex);
            session->read_timer = ktime_set(0, param*1000000); /* nsecs*1000000 -> param in milliseconds */
            DEBUG
                printk("%s: ioctl() has set SET_RECV_TIMEOUT:%llu\n", MODNAME, session->read_timer);
            mutex_unlock(&session->operation_mutex);
            break;
        case REVOKE_DELAYED_MESSAGES: //27394
            DEBUG
                printk("%s: ioctl() has called REVOKE_DELAYED_MESSAGES\n", MODNAME);
            mutex_lock(&instance_by_minor[minor].dev_mutex);
            __del_all_deferred_writes(minor);
            mutex_unlock(&instance_by_minor[minor].dev_mutex);
            break;
        case DELETE_ALL_MESSAGES: //27395
            DEBUG
                printk("%s: ioctl() has called DELETE_ALL_MESSAGES\n", MODNAME);
            mutex_lock(&instance_by_minor[minor].dev_mutex);
            __del_all_messages(minor);
            mutex_unlock(&instance_by_minor[minor].dev_mutex);
            break;
        case FLUSH_DEF_WORK: //27396
            DEBUG
                printk("%s: ioctl() has called FLUSH_DEF_WORK\n", MODNAME);
            mutex_lock(&instance_by_minor[minor].dev_mutex);
            __del_all_deferred_works(minor);
            mutex_unlock(&instance_by_minor[minor].dev_mutex);
            break;
        default:
            printk(KERN_ERR "%s: ioctl() has been called with unexpected command: %u\n", MODNAME, command);
            return -EINVAL; /* Invalid argument */
    }
    return 0;
}


static int dev_flush(struct file *file, fl_owner_t owner){

    int minor = get_minor(file);

    DEBUG
        printk("%s: someone called a FLUSH on dev with minor number %d\n", MODNAME, minor);

    //flush all deferred works
    mutex_lock(&instance_by_minor[minor].dev_mutex);
    __del_all_deferred_works(minor);
    mutex_unlock(&instance_by_minor[minor].dev_mutex);

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

    //initialize the driver internal state -> global variables per minor
    for(i=0; i<MINORS; i++){

        //init messages's data struct
        instance_by_minor[i].actual_total_size = 0;
        INIT_LIST_HEAD(&instance_by_minor[i].stored_messages);

        //init list of active sessions per minor
        INIT_LIST_HEAD(&instance_by_minor[i].all_sessions);

        mutex_init(&instance_by_minor[i].dev_mutex);

        //deferred read structs
        instance_by_minor[i].num_pending_read = 0;
        init_waitqueue_head(&instance_by_minor[i].deferred_read);
        INIT_LIST_HEAD(&instance_by_minor[i].readers_subscriptions);
    }

    Major = __register_chrdev(0, 0, 256, DEVICE_NAME, &fops);
    if (Major < 0) {
        printk(KERN_ERR "%s: registering device failed\n",MODNAME);
        return Major;
    }

    printk(KERN_INFO "%s: New device registered, it is assigned major number %d\n", MODNAME, Major);
    return 0;

}


static void __exit remove_dev(void){
    int i;

    //removing all driver internal state's variables
    for(i = 0; i < MINORS; i++) {
        __del_all_messages(i);
        list_del(&instance_by_minor[i].stored_messages);
        list_del(&instance_by_minor[i].all_sessions);
    }

    unregister_chrdev(Major, DEVICE_NAME);
    printk(KERN_INFO "%s: New device unregistered, it was assigned major number %d\n",MODNAME, Major);
    return;
}

module_init(add_dev);
module_exit(remove_dev);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("dilettalagom");
MODULE_DESCRIPTION("CoreMess module.");
MODULE_VERSION("0.01");