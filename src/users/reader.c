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
    unsigned int major, minor;
    unsigned long read_timer;
    char* mess;

    if(argc != 5){
        fprintf(stderr, "Usage: sudo %s <filename> <major> <minor> <read_timer_micros>\n", argv[0]);
        return(EXIT_FAILURE);
    }
    major = strtoul(argv[2], NULL, 10);
    minor = strtoul(argv[3], NULL, 10);

    //Create a char device file with the given major and 0 with minor number
//    ret = mknod(argv[1], S_IFCHR, makedev(major, minor));
//    if (ret == -1) {
//        fprintf(stderr, "mknod() failed\n");
//        return(EXIT_FAILURE);
//    }

    // Opening the input file
    fd = open(argv[1], O_RDONLY);
    if(fd < 0){
        fprintf(stderr, "Could not open the file: %s\n", strerror(errno));
        return(EXIT_FAILURE);
    }
    fprintf(stdout, "File device opened with fd: %d\n", fd);

    //Setting of read_timer
    read_timer = strtoul(argv[4], NULL, 10);
    if(read_timer != 0) {
        ret = ioctl(fd, SET_RECV_TIMEOUT, read_timer);
        if (ret == -EINVAL) {
            fprintf(stderr, "ioctl() failed: %s\n", strerror(errno));
            return (EXIT_FAILURE);
        }
    }

    mess = malloc(sizeof(char)*MAX_MESSAGE_SIZE);

    // Reading new messagges from file
    while (1) {

        ret = read(fd, mess, MAX_MESSAGE_SIZE);
        switch (ret) {
            case -1:
                fprintf(stdout, "There aren't new messages for you. Sorry.\n");
                break;
            case -ERESTART:
                fprintf(stderr, "An error occurred (ERESTART): %s\n", strerror(errno));
                break;
            case -EINVAL:
                fprintf(stderr, "An error occurred (EINVAL): %s\n", strerror(errno));
                break;
            default:
                fprintf(stdout, "You have a new message: %s\n", mess);
                break;
        }
        fflush(stdout);
        sleep(10);
    }
    return EXIT_SUCCESS;
}