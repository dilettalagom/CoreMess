//gcc -pthread 4_rekove_messages_test.c -o run_test_4.o

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

#define num_writers 5
#define write_timer 5
#define revoke_timer write_timer-2

pthread_t writers[num_writers];

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
        ret = ioctl(fd, SET_SEND_TIMEOUT, write_timer*1000);
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


int main(int argc, char *argv[]) {

    int ret, fd;
    unsigned int major;
    char *mess_read;


    if (argc != 2) {
        fprintf(stderr, "Usage: sudo %s <major>\n", argv[0]);
        return (EXIT_FAILURE);
    }
    major = strtoul(argv[1], NULL, 10);

    //creating a char device file with the given major and 0 with minor number
    ret = mknod(FILENAME, S_IFCHR, makedev(major, TEST_MINOR));
    if (ret == -1) {
        fprintf(stderr, "mknod() failed: %s\n", strerror(errno));
        return (EXIT_FAILURE);
    }

    fprintf(stdout, "\nRevoking all deferred messages using ioctl:REVOKE_DELAYED_MESSAGES.\n"
                    "All deferred writes will wait %ds before materializing the message:\n"
                    "the revoke will be invoked after %ds from spawning them.\n"
                    "Than will be checked if any deferred write has been materialized anyway.\n", write_timer, revoke_timer);

    //creating readers and writers threads
    if (num_writers > 0) {
        for (int i = 0; i < num_writers; i++) {
            if (pthread_create(&writers[i], NULL, &writer_routine, NULL)) {
                fprintf(stderr, "pthread_create() failed\n");
                return (EXIT_FAILURE);
            }
        }
    }

    //waiting for all deferred work to be setup
    fprintf(stdout, "\nSleeping %ds than checking if any deferred message has been materialized.\n",revoke_timer);
    sleep(revoke_timer);

    //revoking all deferred messages
    fd = open(FILENAME, O_RDWR);
    if (fd < 0) {
        fprintf(stderr, "open() failed: %s\n", strerror(errno));
        return (EXIT_FAILURE);
    }

    ret = ioctl(fd, REVOKE_DELAYED_MESSAGES);
    if (ret < 0) {
        fprintf(stderr, "ioctl(FLUSH_DEF_WORK) failed: %s\n", strerror(errno));
        return (EXIT_FAILURE);
    }

    if (NUM_WRITERS > 0) {
        for (int i = 0; i < NUM_WRITERS; i++) {
            pthread_join(writers[i], NULL);
        }
    }

    fprintf(stdout, "\nInvoking a non-blocking read. There should not be any message (\"\", 0)\n");
    mess_read = malloc(sizeof(char) * MAX_MESSAGE_SIZE);
    ret = read(fd, mess_read, strlen(mess_read));
    if (ret < 0) {
        fprintf(stderr, "read() failed: %s\n", strerror(errno));
        close(fd);
        return EXIT_FAILURE;
    }
    fprintf(stdout, "read() has been invoked and returned: \"%s\" with ret=%lu\n", mess_read, (unsigned long) ret);

    close(fd);
    fprintf(stdout, "\nFile %s and %d closed.\n", FILENAME, fd);

    //removing the device file
    ret = remove(FILENAME);
    if (ret != 0) {
        printf("Error: unable to delete the file.\n");
        return EXIT_FAILURE;
    }
    printf("File deleted successfully.\n");

    return EXIT_SUCCESS;
}