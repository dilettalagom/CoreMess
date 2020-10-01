#include <linux/mutex.h>
#include <linux/list.h>
#include <linux/ktime.h>
#include <linux/wait.h>
#include <linux/hrtimer.h>
#include <linux/workqueue.h>

//Unit message
typedef struct message_t{
    char* text;
    size_t len;
    struct list_head next;
}message_t;

//Deferred reader's subscription
typedef struct read_subscription_t{
    bool flush_me;
    struct list_head next;
}read_subscription_t;

//Deferred writer's data structure
typedef struct pending_write_t{
    int minor;
    message_t* pending_message;
    struct delayed_work the_deferred_write_work;
    struct list_head next;
}pending_write_t;

//Metadati privati per singola sessione -> più single_session per stesso (major, minor)
//rappresenta un singolo user utente (writer o reader)
typedef struct single_session{
    struct mutex operation_mutex;
    unsigned long write_timer;
    ktime_t read_timer;
    struct list_head pending_defwrite_structs; //workstruct for deferred messages that must be stored in <stored_messages>
    struct workqueue_struct* pending_write_wq;
    struct list_head readers_subscriptions; //condition for flushing readers
    struct list_head next;
}single_session;

//Metadati globali per fd -> uno per ogni MINOR
//rappresenta una istanza del driver
typedef struct device_instance{

    unsigned long actual_total_size;
    struct list_head stored_messages; //messages already stored
    struct list_head all_sessions;
    struct mutex dev_mutex;     //makes access to global structures unique
    long num_pending_read; //condition for wakeup readers
    wait_queue_head_t deferred_read; //wait_queue dinamically allocated
    //    struct list_head readers_subscriptions; //condition for flushing readers

}device_instance;
