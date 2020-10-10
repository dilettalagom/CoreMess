#include "../include/config.h"
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <signal.h>
#include <stdbool.h>

int fd;

void sig_handler(int sig_num){
    fprintf(stdout, "ReaderProcess has been stopped. See ya.\n");
    close(fd);
    exit(EXIT_SUCCESS);
}

int main(int argc, char *argv[]){

    int ret;
    bool exit = true;
    unsigned long read_timer;
    char* mess;

    if(argc != 3){
        fprintf(stderr, "Usage: sudo %s <filename> <read_timer_millis>\n", argv[0]);
        return(EXIT_FAILURE);
    }

    // Opening the input file
    fd = open(argv[1], O_RDONLY);
    if(fd < 0){
        fprintf(stderr, "Could not open the file: %s\n", strerror(errno));
        return(EXIT_FAILURE);
    }
    fprintf(stdout, "File device opened with fd: %d\n", fd);

    //Getting read_timer value
    read_timer = strtoul(argv[2], NULL, 10);
    if (read_timer<0){
        fprintf(stdout, "Please, choose a read_timer_millis>0.\n");
        return EXIT_FAILURE;
    }
    fprintf(stdout, "read_timer: %lu\n", read_timer);

    mess = malloc(sizeof(char)*MAX_MESSAGE_SIZE);
    signal(SIGTSTP, sig_handler);
    signal(SIGINT, sig_handler);

    // Reading new messagges from file
    while (exit) {

        //Setting of read_timer
        if(read_timer != 0) {
            ret = ioctl(fd, SET_RECV_TIMEOUT, read_timer);
            if (ret == -EINVAL) {
                fprintf(stderr, "ioctl() failed: %s\n", strerror(errno));
                return (EXIT_FAILURE);
            }
        }

        ret = read(fd, mess, MAX_MESSAGE_SIZE);
        if (ret <= 0) {
            fprintf(stdout, "There aren't new messages. Sorry. %s\n", strerror(errno));
            if (errno == EINTR) exit = false;
        }
        else
            fprintf(stdout, "You have a new message: %s. ret=%d\n", mess, ret);
        fflush(stdout);
        sleep(2);
    }
    printf("Exiting...\n");
    close(fd);
    return EXIT_FAILURE;
}
