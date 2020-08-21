
#include <linux/mutex.h>
#include <linux/list.h>

//Singolo messaggio
typedef struct message_t{
    char* message;
    unsigned long release_timer;
    struct list_head next;
}message_t;


//Metadati privati per singola sessione -> più single_session per stesso (major, minor)
//rappresenta un singolo user utente (writer o rider)
typedef struct single_session{
    unsigned long write_timer;
    unsigned long read_timer;
    struct list_head next;

}single_session;

//Metadati globali per fd -> uno per ogni MINOR
//rappresenta una istanza del driver
typedef struct device_instance{
/*    struct mutex object_busy;
    struct mutex operation_synchronizer;
    int valid_bytes;
    char* stream_content;//the I/O node is a buffer in memory*/

    unsigned int actual_total_size;
    struct list_head stored_messages;

    struct list_head all_sessions;

    struct mutex write_mutex;
    struct mutex read_mutex;
    struct list_head deferred_write_messages;


} device_instance;