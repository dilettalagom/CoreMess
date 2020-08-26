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

    int fd, ret, ret_read;
    unsigned int major, minor;
    unsigned long write_timer;
    char mess[MAX_MESSAGE_SIZE];
    char* mess_cleaned = NULL, *mess_read;
    unsigned long len;

    if(argc != 4){
        fprintf(stderr, "Usage: sudo %s <filename> <major> <minor>\n", argv[0]);
        return(EXIT_FAILURE);
    }
    major = strtoul(argv[2], NULL, 10);
    minor = strtoul(argv[3], NULL, 10);

    // Create a char device file with the given major and 0 with minor number
    ret = mknod(argv[1], S_IFCHR, makedev(major, minor));
    if (ret == -1) {
        fprintf(stderr, "mknod() failed\n");
        return(EXIT_FAILURE);
    }

    // Opening the input file
    fd = open(argv[1], O_RDWR);
    if (fd < 0) {
        fprintf(stderr, "open() failed: %s\n", strerror(errno));
        return (EXIT_FAILURE);
    }
    fprintf(stdout, "File device opened with fd: %d\n", fd);

    while (!feof(stdin)) {

        //Getting input_command or message_to_send to device driver
        fprintf(stdout, "\nWrite a new message or a dev_command (SEND_TIMEOUT/REVOKE/QUIT)\n");
        if(fscanf(stdin, "%s", mess)<0){
            fprintf(stderr,"Error in scanf: %s\n",strerror(errno));
            exit(EXIT_FAILURE);
        }
        len = strlen(mess);
        mess[len]='\0';
        len +=1;

        //TODO:Revoke all messagges
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
            if(fscanf(stdin, "%s", mess)<0){
                fprintf(stderr,"Error in scanf: %s\n",strerror(errno));
                break;
            }
            write_timer = strtoul(mess, NULL, 10);
            ret = ioctl(fd, SET_SEND_TIMEOUT, write_timer);
            if(ret<0){
                fprintf(stderr, "ioctl(SET_SEND_TIMEOUT) failed: %s\n", strerror(errno));
                close(fd);
                return EXIT_FAILURE;
            }
        //Releasing file
        }else if(strcmp(mess, "QUIT") == 0){
                close(fd);
                free(mess_cleaned);
                fprintf(stdout, "File %s and %d cloesd. See ya.\n", argv[1], fd);
                return EXIT_SUCCESS;
        //Writing on file
        }else {

            //clean-up buffer unused characters
            mess_cleaned = calloc(len, sizeof(char));
            if(mess_cleaned == NULL){
                fprintf(stderr, "calloc failed: %s\n", strerror(errno));
                return EXIT_FAILURE;
            }
            memccpy(mess_cleaned, mess, '\0', len);

            // Write the input into the device file
            fprintf(stdout, "Sending message to device: %s , %lu\n", mess_cleaned, len);
            ret = write(fd, mess, len);

            fprintf(stdout, "write() returned: ret=%d\n", ret);

            sleep(1);

            mess_read = calloc(MAX_MESSAGE_SIZE, sizeof(char));
            ret_read = read(fd, mess_read, MAX_MESSAGE_SIZE);
            if (ret_read < 0) {
                fprintf(stderr, "Could not read a new message: %s\n", strerror(errno));
                return EXIT_FAILURE;
            }
            fprintf(stdout, "read() returned: %s, ret=%d\n", mess_read, ret_read);

        }
    }

    close(fd);
    return(EXIT_SUCCESS);

}
