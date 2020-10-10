//gcc -pthread 5_flushing_defworks_test.c -o run_test_5.o
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

    //invoking a deferred write
    if(WRITER_TIMER != 0) {
        ret = ioctl(fd, SET_SEND_TIMEOUT, WRITER_TIMER);
        if (ret < 0) {
            fprintf(stderr, "ioctl(SET_SEND_TIMEOUT) failed: %s\n", strerror(errno));
            pthread_exit(NULL);
        }
    }
    for(i=0; i<NUM_MESSAGES; i++){
        sprintf(mess, "%d%c", i, '\0');
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

    //invoking a deferred read
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

    int ret, fd;
    unsigned int major;
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


    fprintf(stdout, "\nFlushing all deferred work using ioctl:FLUSH_DEF_WORK.\n"
                    "-all deferred readers will return with error \"Interrupted system call\";\n"
                    "-all deferred writers will return with ret=0;\n"
                    "-all deferred messages will be deleted.\n"
                    "Creating readers and writers...\n\n");

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

    //waiting for all deferred work to be setup
    sleep(1);

    //revoking all deferred write
    fd = open(FILENAME, O_RDWR);
    if (fd < 0) {
        fprintf(stderr, "open() failed: %s\n", strerror(errno));
        return (EXIT_FAILURE);
    }

    ret = ioctl(fd, FLUSH_DEF_WORK);
    if (ret < 0) {
        fprintf(stderr, "ioctl(FLUSH_DEF_WORK) failed: %s\n", strerror(errno));
        return (EXIT_FAILURE);
    }

    fprintf(stdout, "\nSleeping 5s than checking if any deferred write has been materialized.\n");
    sleep(5);
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

    fprintf(stdout, "\nInvoking a non-blocking read. There should not be any messages (\"\", 0)\n");
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