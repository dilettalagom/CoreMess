//gcc 2_defwrite_read_test.c -o run_test_2.o
#include <stdio.h>
#include <stdlib.h>
#include <sys/sysmacros.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include "./test_conf.h"
#include "../../include/config.h"

int main(int argc, char *argv[]){

    int ret, ret_read=0, fd;
    unsigned int major;
    unsigned long writer_timer = 5000; //5 secondi
    char* mess_write = "new_message\0";
    char* mess_read;

    if(argc != 2){
        fprintf(stderr, "Usage: sudo %s <major>\n", argv[0]);
        return(EXIT_FAILURE);
    }
    major = strtoul(argv[1], NULL, 10);

    //creating a char device file with the given major and 0 with minor number
    ret = mknod(FILENAME, S_IFCHR, makedev(major, TEST_MINOR));
    if (ret == -1) {
        fprintf(stderr, "mknod() failed: %s\n", strerror(errno));
        return(EXIT_FAILURE);
    }
    fprintf(stdout, "File device %s created with (%d,%d)\n", FILENAME, major, TEST_MINOR);

    //opening the input file
    fd = open(FILENAME, O_RDWR);
    if (fd < 0) {
        fprintf(stderr, "open() failed: %s\n", strerror(errno));
        return (EXIT_FAILURE);
    }
    fprintf(stdout, "File device opened with fd: %d\n", fd);

    //creating a deferred write
    fprintf(stdout, "\nAdding a messages with a deferred write.\n");
    ret = ioctl(fd, SET_SEND_TIMEOUT, writer_timer);
    if (ret < 0) {
        fprintf(stderr, "ioctl(SET_SEND_TIMEOUT) failed: %s\n", strerror(errno));
        return EXIT_FAILURE;
    }
    fprintf(stdout, "SET_SEND_TIMEOUT set with %lu ms\n", writer_timer);

    ret = write(fd, mess_write, strlen(mess_write));
    if(ret<0){
        fprintf(stderr, "write() failed: %s\n", strerror(errno));
        close(fd);
        return EXIT_FAILURE;
    }
    fprintf(stdout, "write() has been invoked and returned with ret=%lu (must be ret=0)\n", (unsigned long) ret);

    //keep reading until a message is actually received
    fprintf(stdout, "\nKeep invoking a non-blocking read until a message is received.\n"
                    "Result should be (new_message, 11)\n");
    while (ret_read<=0){
        mess_read = malloc(sizeof(char)*MAX_MESSAGE_SIZE);
        ret_read = read(fd, mess_read, 11);
        fprintf(stdout, "There aren't new messages. Sorry.\n");
        sleep(1);
    }
    fprintf(stdout, "read() has been invoked and returned: \"%s\" with ret=%lu\n", mess_read, (unsigned long) ret_read);

    close(fd);
    fprintf(stdout, "\nFile %s and %d closed.\n", FILENAME, fd);

    //removing the device file
    ret = remove(FILENAME);
    if(ret != 0) {
        printf("Error: unable to delete the file.\n");
        return EXIT_FAILURE;
    }
    printf("File deleted successfully.\n");

    return EXIT_SUCCESS;
}


