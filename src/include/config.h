#ifndef CONFIG_H
#define CONFIG_H


//Messages default values
#define MAX_MESSAGE_SIZE 128       //byte. This can be configured via the sys file system
#define MAX_STORAGE_SIZE 1024000  //byte (1MB). This can be configured via the sys file system

//ioctl commands
#define IOC_BASE_NUM 'k'
#define SET_SEND_TIMEOUT         _IO(IOC_BASE_NUM, 0)
#define SET_RECV_TIMEOUT         _IO(IOC_BASE_NUM, 1)
#define REVOKE_DELAYED_MESSAGES  _IO(IOC_BASE_NUM, 2)
#define DELETE_ALL_MESSAGES      _IO(IOC_BASE_NUM, 3)

#endif