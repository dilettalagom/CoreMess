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

    int fd, ret, ret_read;
    unsigned int major, minor;
    unsigned long write_timer;
    char* mess;
    char* mess_read;
    unsigned long len;

    if(argc != 5){
        fprintf(stderr, "Usage: sudo %s <filename> <major> <minor> <write_timer_micros>\n", argv[0]);
        return(EXIT_FAILURE);
    }
    major = strtoul(argv[2], NULL, 10);
    minor = strtoul(argv[3], NULL, 10);

    // Create a char device file with the given major and 0 with minor number
    ret = mknod(argv[1], S_IFCHR, makedev(major, minor));
    if (ret == -1) {
        fprintf(stderr, "mknod() failed\n");
        return(EXIT_FAILURE);
    }

    // Opening the input file
    fd = open(argv[1], O_RDWR);
    if(fd < 0) {
        fprintf(stderr, "open() failed: %s\n", strerror(errno));
        return(EXIT_FAILURE);
    }
    fprintf(stdout, "File device opened with fd: %d\n", fd);

    //Setting of write_timer
    write_timer = strtoul(argv[4], NULL, 10);
    if(write_timer != 0) {
        ret = ioctl(fd, SET_SEND_TIMEOUT, write_timer);
        if (ret == -EINVAL) {
            fprintf(stderr, "ioctl() has failed: %s\n", strerror(errno));
            return (EXIT_FAILURE);
        }
    }

    /*TODO: Reading from STDIN content to write on file*/
    while (!feof(stdin)) {
        mess = malloc(sizeof(char)*MAX_MESSAGE_SIZE);
        mess_read = malloc(sizeof(char)*MAX_MESSAGE_SIZE);


        if(fscanf(stdin, "%s", mess)<0){
            fprintf(stderr,"Error in scanf: %s\n",strerror(errno));
            exit(EXIT_FAILURE);
        }
        len = strlen(mess);
        mess = realloc(mess, len);


        /*if (strcmp(mess, "REVOKE_DELAYED_MESSAGES") == 0) {
            ret = ioctl(fd, REVOKE_DELAYED_MESSAGES);
            if (ret == -1) {
                fprintf(stderr, "revoke delayed messages failed: %s\n", strerror(errno));
            } else {
                printf("delayed messages have been revoked\n");
            }
            continue;
        }
        if (strcmp(mess, "CLOSE") == 0) {
            close(fd);
            printf("File descriptor closed\n");
            return(EXIT_SUCCESS);
        }*/
        // Write the input into the device file
        fprintf(stdout, "Sending message to device: %s , %lu\n", mess, len);
        ret = write(fd, mess, strlen(mess));

        fprintf(stdout, "write(): %d\n", ret);

    //        ret_read = read(fd, mess_read, MAX_MESSAGE_SIZE);
    //        if (ret_read < 0) {
    //            fprintf(stderr, "Could not read a new message: %s\n", strerror(errno));
    //        } else {
    //            printf("You have a new message: %s\n", mess_read);
    //        }
    }

    close(fd);
    return(EXIT_SUCCESS);

}
