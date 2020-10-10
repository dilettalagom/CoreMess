//gcc -pthread 3_defread_write_test.c -o run_test_3.o
#include <stdio.h>
#include <stdlib.h>
#include <sys/sysmacros.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/ioctl.h>
#include "./test_conf.h"
#include "../../include/config.h"

#define num_readers 5
pthread_t readers[num_readers];
int fd;

void* reader_routine(void* args){

    int ret;
    pthread_t t_id = pthread_self();
    char* mess = malloc(sizeof(char)*MAX_MESSAGE_SIZE);

    ret = read(fd, mess, MAX_MESSAGE_SIZE);
    if (ret <= 0)
        fprintf(stdout, "READER %lu: NO messages. errno=%s\n", t_id, strerror(errno));
    else
        fprintf(stdout, "READER %lu: New message: \"%s\". ret=%d\n", t_id, mess, ret);    pthread_exit(EXIT_SUCCESS);
}

int main(int argc, char *argv[]){

    int ret, ret_read=0;
    unsigned int major;
    unsigned long read_timer = 5000; //5 secondi
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

    fprintf(stdout, "\nInvoking %d deferred reads:\n"
                    "1)one will receive the message;\n"
                    "2)the others will return with error \"Timer expired\".\n", num_readers);

    //creating the deferred reads
    ret = ioctl(fd, SET_RECV_TIMEOUT, read_timer);
    if (ret < 0) {
        fprintf(stderr, "ioctl(SET_RECV_TIMEOUT) failed: %s\n", strerror(errno));
        return EXIT_FAILURE;
    }
    fprintf(stdout, "\nSET_RECV_TIMEOUT set with %lu ms\n", read_timer);

    for (int i = 0; i < num_readers; i++) {
        if (pthread_create(&readers[i], NULL, &reader_routine, NULL)) {
            fprintf(stderr, "pthread_create() failed\n");
            return (EXIT_FAILURE);
        }
    }

    //creating a non-blocking write
    ret = write(fd, mess_write, strlen(mess_write));
    if(ret<0){
        fprintf(stderr, "write() failed: %s\n", strerror(errno));
        close(fd);
        return EXIT_FAILURE;
    }
    fprintf(stdout, "\nwrite() has been invoked and returned with ret=%lu (must be ret>0)\n", (unsigned long) ret);

    for (int i = 0; i < num_readers; i++)
        pthread_join(readers[i], NULL);

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


