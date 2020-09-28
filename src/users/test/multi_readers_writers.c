#include <stdio.h>
#include <stdlib.h>
#include <sys/sysmacros.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include "./test_conf.h"

int fd;
pthread_t readers[NUM_READERS];
pthread_t writers[NUM_WRITERS];

void sig_handler(int sig_num){
    fprintf(stdout, "Test stopped.\n");
    close(fd);
    exit(EXIT_SUCCESS);
}

void* writer_routine(void* args){

    int i, ret;
    char mess[MAX_MESSAGE_SIZE];
    pthread_t t_id = pthread_self();

    for(i=0; i<NUM_MESSAGES; i++){
        sprintf(mess, "%d", i);
        ret = write(fd, mess, sizeof(i));
        fprintf(stdout, "WRITER %lu: write() returned: ret=%d\n", t_id, ret);
        sleep(1);
    }

    pthread_exit(EXIT_SUCCESS);
}

void* reader_routine(void* args){

    int i, ret;
    pthread_t t_id = pthread_self();
    char* mess = malloc(sizeof(char)*MAX_MESSAGE_SIZE);

    for(i=0; i<NUM_MESSAGES; i++){
        ret = read(fd, mess, MAX_MESSAGE_SIZE);
        if (ret <= 0)
            fprintf(stdout, "READER %lu: NO messages.%s\n", t_id, strerror(errno));
        else
            fprintf(stdout, "READER %lu: New message:%s. ret=%d\n", t_id, mess, ret);
        sleep(2);
    }

    pthread_exit(EXIT_SUCCESS);
}

int main(int argc, char *argv[]){

    int ret;
    unsigned int major;

    if(argc != 3){
        fprintf(stderr, "Usage: sudo %s <filename> <major>\n", argv[0]);
        return(EXIT_FAILURE);
    }
    major = strtoul(argv[2], NULL, 10);

    // Create a char device file with the given major and 0 with minor number
    ret = mknod(argv[1], S_IFCHR, makedev(major, TEST_MINOR));
    if (ret == -1) {
        fprintf(stderr, "mknod() failed: %s\n", strerror(errno));
        return(EXIT_FAILURE);
    }

    // Opening the input file
    fd = open(argv[1], O_RDWR);
    if (fd < 0) {
        fprintf(stderr, "open() failed: %s\n", strerror(errno));
        return (EXIT_FAILURE);
    }
    fprintf(stdout, "File device opened with fd: %d\n", fd);


    // Creating readers and writers threads
    for (int i = 0; i < NUM_WRITERS; i++) {
        if (pthread_create(&writers[i], NULL, &writer_routine, NULL)) {
            fprintf(stderr, "pthread_create() failed\n");
            return(EXIT_FAILURE);
        }
    }

    for (int i = 0; i < NUM_READERS; i++) {
        if (pthread_create(&readers[i], NULL, &reader_routine, NULL)) {
            fprintf(stderr, "pthread_create() failed\n");
            return(EXIT_FAILURE);
        }
    }

    for (int i=0; i<NUM_WRITERS; i++){
        pthread_join(writers[i], NULL);
    }

    for (int i=0; i<NUM_READERS; i++){
        pthread_join(readers[i], NULL);
    }

    close(fd);
    fprintf(stdout, "File %s and %d cloesd.\n", argv[1], fd);

    return EXIT_SUCCESS;
}