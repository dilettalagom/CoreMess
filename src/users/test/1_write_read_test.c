//gcc 1_write_read_test.c -o run_test_1.o
#include <stdio.h>
#include <stdlib.h>
#include <sys/sysmacros.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include "./test_conf.h"

int main(int argc, char *argv[]){

    int ret, fd;
    unsigned int major;
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

    //creating two non-blocking write
    fprintf(stdout, "\nAdding two messages with two non-blocking write.\n");

    ret = write(fd, mess_write, strlen(mess_write));
    if(ret<0){
        fprintf(stderr, "write() failed: %s\n", strerror(errno));
        close(fd);
        return EXIT_FAILURE;
    }
    fprintf(stdout, "write() has been invoked and returned with ret=%lu (must be ret>0)\n", (unsigned long) ret);

    ret = write(fd, mess_write, strlen(mess_write));
    if(ret<0){
        fprintf(stderr, "write() failed: %s\n", strerror(errno));
        close(fd);
        return EXIT_FAILURE;
    }
    fprintf(stdout, "write() has been invoked and returned with ret=%lu (must be ret>0)\n", (unsigned long) ret);

    //creating first non-blocking read
    fprintf(stdout, "\nInvoking first non-blocking read.\n"
                    "Result should be (new_message, 11)\n");
    mess_read = malloc(sizeof(char)*MAX_MESSAGE_SIZE);
    ret = read(fd, mess_read, 11);
    if(ret<0){
        fprintf(stderr, "read() failed: %s\n", strerror(errno));
        close(fd);
        return EXIT_FAILURE;
    }
    fprintf(stdout, "read() has been invoked and returned: \"%s\" with ret=%lu\n", mess_read, (unsigned long) ret);

    //creating second non-blocking read
    fprintf(stdout, "\nInvoking second non-blocking read with less bytes.\n"
                    "Result should be (new_mes, 7)\n");
    mess_read = malloc(sizeof(char)*MAX_MESSAGE_SIZE);
    ret = read(fd, mess_read, 7);
    if(ret<0){
        fprintf(stderr, "read() failed: %s\n", strerror(errno));
        close(fd);
        return EXIT_FAILURE;
    }
    fprintf(stdout, "read() has been invoked and returned: \"%s\" with ret=%lu\n", mess_read, (unsigned long) ret);

    //creating third non-blocking read
    fprintf(stdout, "\nInvoking third non-blocking read.\n"
                    "There should not be any messages (\"\", 0)\n");
    mess_read = malloc(sizeof(char)*MAX_MESSAGE_SIZE);
    ret = read(fd, mess_read, strlen(mess_read));
    if(ret<0){
        fprintf(stderr, "read() failed: %s\n", strerror(errno));
        close(fd);
        return EXIT_FAILURE;
    }
    fprintf(stdout, "read() has been invoked and returned: \"%s\" with ret=%lu\n", mess_read, (unsigned long) ret);

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
