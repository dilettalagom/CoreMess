#include <linux/mutex.h>
#include <linux/list.h>
#include <linux/ktime.h>

//Singolo messaggio
typedef struct message_t{
    char* text;
    unsigned long release_timer;
    struct list_head next;
}message_t;


//Metadati privati per singola sessione -> più single_session per stesso (major, minor)
//rappresenta un singolo user utente (writer o rider)
typedef struct single_session{
    //unsigned long write_timer
    ktime_t write_timer;
    //unsigned long read_timer;
    ktime_t read_timer;
    struct list_head next;
    struct mutex operation_mutex;

}single_session;

//Metadati globali per fd -> uno per ogni MINOR
//rappresenta una istanza del driver
typedef struct device_instance{

    unsigned long actual_total_size;
    struct list_head stored_messages;
    struct list_head all_sessions;
    struct mutex dev_mutex;     //makes access to global structures unique
    struct list_head deferred_write_messages;


} device_instance;