#include "../include/config.h"
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <signal.h>

int fd;

void sig_handler(int sig_num){
    fprintf(stdout, "WriterProcess has been stopped. See ya.\n");
    close(fd);
    exit(EXIT_SUCCESS);
}

int main(int argc, char *argv[]){

    int ret;
    unsigned long write_timer;
    char mess[MAX_MESSAGE_SIZE];
    unsigned long len;

    if(argc != 2){
        fprintf(stderr, "Usage: sudo %s <filename>\n", argv[0]);
        return(EXIT_FAILURE);
    }

    signal(SIGTSTP, sig_handler);
    signal(SIGINT, sig_handler);

    // Opening the input file
    fd = open(argv[1], O_RDWR);
    if (fd < 0) {
        fprintf(stderr, "open() failed: %s\n", strerror(errno));
        return (EXIT_FAILURE);
    }
    fprintf(stdout, "File device opened with fd: %d\n", fd);

    while (!feof(stdin)) {

        //Getting input_command or message_to_send to device driver
        fprintf(stdout, "\nWrite a new message or a dev_command (SEND_TIMEOUT/REVOKE/DELETE/FLUSH/QUIT)\n");
        if(fscanf(stdin, "%s", mess)<0){
            fprintf(stderr,"Error in scanf: %s\n",strerror(errno));
            exit(EXIT_FAILURE);
        }
        len = strlen(mess);
        //clean-up buffer unused characters
        mess[len]='\0';
        len +=1;

        //Revoke all messagges
        if(strcmp(mess, "REVOKE") == 0){
            ret = ioctl(fd, REVOKE_DELAYED_MESSAGES);
            if(ret<0){
                fprintf(stderr, "ioctl(REVOKE_DELAYED_MESSAGES) failed: %s\n", strerror(errno));
                close(fd);
                return EXIT_FAILURE;
            }
        //Setting of write_timer
        }else if(strcmp(mess, "SEND_TIMEOUT") == 0) {
            fprintf(stdout, "Please, insert SET_SEND_TIMEOUT value: ");
            if (fscanf(stdin, "%s", mess) < 0) {
                fprintf(stderr, "Error in scanf: %s\n", strerror(errno));
                break;
            }
            write_timer = strtoul(mess, NULL, 10);
            if (write_timer<0) {
                fprintf(stdout, "Please, choose a write_timer_micros>0.\n");
                close(fd);
                return EXIT_FAILURE;
            }
            ret = ioctl(fd, SET_SEND_TIMEOUT, write_timer);
            if (ret < 0) {
                fprintf(stderr, "ioctl(SET_SEND_TIMEOUT) failed: %s\n", strerror(errno));
                close(fd);
                return EXIT_FAILURE;
            }
        //Delete all stored messages
        }else if (strcmp(mess, "DELETE") == 0 ){
            ret = ioctl(fd, DELETE_ALL_MESSAGES);
            if(ret<0){
                fprintf(stderr, "ioctl(DELETE_ALL_MESSAGES) failed: %s\n", strerror(errno));
                close(fd);
                return EXIT_FAILURE;
            }
        //Flushing all deferred work
        }else if (strcmp(mess, "FLUSH") == 0 ){
            ret = ioctl(fd, FLUSH_DEF_WORK);
            if(ret<0){
                fprintf(stderr, "ioctl(FLUSH_DEF_WORK) failed: %s\n", strerror(errno));
                close(fd);
                return EXIT_FAILURE;
            }
        //Releasing file
        }else if(strcmp(mess, "QUIT") == 0){
            close(fd);
            fprintf(stdout, "File %s and %d cloesd. See ya.\n", argv[1], fd);
            return EXIT_SUCCESS;
        //Writing on file
        }else {
            // Write the input into the device file
            fprintf(stdout, "Sending message to device: %s, %lu\n", mess, len);
            ret = write(fd, mess, len);
            if(ret<0){
                fprintf(stderr, "write() failed: %s\n", strerror(errno));
                close(fd);
                return EXIT_FAILURE;
            }
            fprintf(stdout, "write() returned: ret=%d\n", ret);
        }
    }
    return(EXIT_SUCCESS);
}
