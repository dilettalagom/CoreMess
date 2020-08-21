#include "../include/config.h"
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>

int main(int argc, char *argv[]){

    int fd, ret;
    unsigned long write_timer;
    char mess[MAX_MESSAGE_SIZE];

    if(argc != 3) {
        fprintf(stderr, "Usage: sudo %s <filename> <write_timer>\n", argv[0]);
        return(EXIT_FAILURE);
    }

    // Opening the input file
    fd = open(argv[1], O_RDWR);
    if(fd < 0) {
        fprintf(stderr, "open() failed: %s\n", strerror(errno));
        return(EXIT_FAILURE);
    }

    //Setting of write_timer
    write_timer = strtoul(argv[2], NULL, 10);
    if(write_timer != 0) {
        ret = ioctl(fd, SET_RECV_TIMEOUT, write_timer);
        if (ret == -EINVAL) {
            fprintf(stderr, "ioctl() has failed: %s\n", strerror(errno));
            return (EXIT_FAILURE);
        }
    }

    /*TODO: Reading from STDIN content to write on file
    while (1) {
        fputc('>', stdout);
        fflush(stdout);
        if (!fgets(msg, MAX_MESSAGE_SIZE, stdin)) {
            fprintf(stderr, "fgets() failed\n");
            return(EXIT_FAILURE);
        }
        msg[strlen(msg)-1] = '\0';
        if (strcmp(msg, "REVOKE_DELAYED_MESSAGES") == 0) {
            ret = ioctl(fd, REVOKE_DELAYED_MESSAGES);
            if (ret == -1) {
                fprintf(stderr, "revoke delayed messages failed: %s\n", strerror(errno));
            } else {
                printf("delayed messages have been revoked\n");
            }
            continue;
        }
        if (strcmp(msg, "CLOSE") == 0) {
            close(fd);
            printf("File descriptor closed\n");
            return(EXIT_SUCCESS);
        }
        // Write the input into the device file
        ret = write(fd, msg, strlen(msg) + 1);
        if (ret == -1) {
            fprintf(stderr, "write() failed: %s\n", strerror(errno));
        } else {
            fprintf(stderr, "write() returned %d\n", ret);
        }
    }*/


}
