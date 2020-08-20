#ifndef CONFIG_H
#define CONFIG_H


//Messages default values
#define MAX_MESSAGE_SIZE 128       //byte. This can be configured via the sys file system
#define MAX_STORAGE_SIZE 1024000  //byte (1MB). This can be configured via the sys file system

//ioctl commands
#define SET_SEND_TIMEOUT        0
#define SET_RECV_TIMEOUT        1
#define REVOKE_DELAYED_MESSAGES 2

#endif