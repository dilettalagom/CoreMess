//gcc -pthread 6_multi_readers_writers_test.c -o run_test_6.o
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

pthread_t readers[NUM_READERS];
pthread_t writers[NUM_WRITERS];

void* writer_routine(void* args){

    int i, ret, fd;
    char mess[MAX_MESSAGE_SIZE];
    pthread_t t_id = pthread_self();

    //creating a new session on minor 0
    fd = open(FILENAME, O_RDWR);
    if (fd < 0) {
        fprintf(stderr, "open() failed: %s\n", strerror(errno));
        pthread_exit(NULL);
    }

    if(WRITER_TIMER != 0) {
        ret = ioctl(fd, SET_SEND_TIMEOUT, WRITER_TIMER);
        if (ret < 0) {
            fprintf(stderr, "ioctl(SET_SEND_TIMEOUT) failed: %s\n", strerror(errno));
            pthread_exit(NULL);
        }
    }

    for(i=0; i<NUM_MESSAGES; i++){
        sprintf(mess, "%lu%c", t_id, '\0');
        ret = write(fd, mess, strlen(mess));
        fprintf(stdout, "WRITER %lu: write() with ret=%d\n", t_id, ret);
        sleep(1);
    }

    pthread_exit(EXIT_SUCCESS);
}

void* reader_routine(void* args){

    int ret, fd;
    pthread_t t_id = pthread_self();
    char* mess = malloc(sizeof(char)*MAX_MESSAGE_SIZE);

    //creating a new session on minor 0
    fd = open(FILENAME, O_RDONLY);
    if (fd < 0) {
        fprintf(stderr, "open() failed: %s\n", strerror(errno));
        pthread_exit(NULL);
    }

    if(READER_TIMER != 0) {
        ret = ioctl(fd, SET_RECV_TIMEOUT, READER_TIMER);
        if (ret == -EINVAL) {
            fprintf(stderr, "ioctl(SET_RECV_TIMEOUT) failed: %s\n", strerror(errno));
            pthread_exit(NULL);
        }
    }

    ret = read(fd, mess, MAX_MESSAGE_SIZE);
    if (ret <= 0)
        fprintf(stdout, "READER %lu: NO messages. %s\n", t_id, strerror(errno));
    else
        fprintf(stdout, "READER %lu: New message:%s. ret=%d\n", t_id, mess, ret);

    pthread_exit(EXIT_SUCCESS);
}

int main(int argc, char *argv[]){

    int ret;
    unsigned int major;

    if(argc != 2){
        fprintf(stderr, "Usage: sudo %s <major>\n", argv[0]);
        return(EXIT_FAILURE);
    }
    major = strtoul(argv[1], NULL, 10);

    // Create a char device file with the given major and 0 with minor number
    ret = mknod(FILENAME, S_IFCHR, makedev(major, TEST_MINOR));
    if (ret == -1) {
        fprintf(stderr, "mknod() failed: %s\n", strerror(errno));
        return(EXIT_FAILURE);
    }
    fprintf(stdout, "File device %s created with (%d,%d)\n", FILENAME, major, TEST_MINOR);


    fprintf(stdout, "\nCreating multiple sessions of deferred reads and writes.\n"
                    "All configuration values can be changed in file \"test_conf.h\" as stress test.\n\n");

    //creating readers and writers threads
    if (NUM_WRITERS>0) {
        for (int i = 0; i < NUM_WRITERS; i++) {
            if (pthread_create(&writers[i], NULL, &writer_routine, NULL)) {
                fprintf(stderr, "pthread_create() failed\n");
                return (EXIT_FAILURE);
            }
        }
    }

    if (NUM_READERS>0) {
        for (int i = 0; i < NUM_READERS; i++) {
            if (pthread_create(&readers[i], NULL, &reader_routine, NULL)) {
                fprintf(stderr, "pthread_create() failed\n");
                return (EXIT_FAILURE);
            }
        }
    }

    if (NUM_WRITERS>0) {
        for (int i = 0; i < NUM_WRITERS; i++) {
            pthread_join(writers[i], NULL);
        }
    }

    if (NUM_READERS>0) {
        for (int i = 0; i < NUM_READERS; i++) {
            pthread_join(readers[i], NULL);
        }
    }

    //removing the device file
    ret = remove(FILENAME);
    if(ret != 0) {
        printf("\nError: unable to delete the file.\n");
        return EXIT_FAILURE;
    }
    printf("\nFile deleted successfully.\n");

    return EXIT_SUCCESS;
}