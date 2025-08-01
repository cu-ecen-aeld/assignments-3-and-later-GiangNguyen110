#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>
#include <string.h>

#include <fcntl.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
    // Open the syslog with a specified identifier
    openlog("writer", LOG_PID | LOG_CONS, LOG_USER);

    if((argc == 2) && strcmp(argv[1], "-h") == 0 )
    {
        printf("Usage: Writing <string> to <file>\r\n");
        return 0;
    }

    if(argc < 3)
    {
        syslog(LOG_ERR,
                "Number of input arguments should be equal to 02. "
                "The first argument is a full path to a file (including filename) on the filesystem. "
                "The second argument is a text string which will be written within this file. ");
        return 1;
    };

    int fd;
    char * dir_path = argv[1];
    char * str_to_write = argv[2];

    fd = open(dir_path, O_WRONLY | O_TRUNC | O_CREAT, 0644);
    if (fd == -1 )
    {
        /* error */
        syslog(LOG_ERR, "Failed to open file '%s'", dir_path);
        return 1;
    }

    syslog(LOG_DEBUG, "Writing '%s' to '%s'", str_to_write, dir_path);

    ssize_t nr;
    nr = write(fd, str_to_write, strlen(str_to_write));
    if(nr == -1)
    {
        perror("Error while writting to file!\r\n");
        return 1;
    }

    closelog();
    return 0;
}