#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "common.h"


int
main(int argc, char *argv[])
{
    if (argc != 2) {
	    fprintf(stderr, "usage: cores <string>\n");
	    exit(1);
    }
    int NUM_CORES = atoi(argv[1]);

    // printf("hello world (pid:%d)\n", (int) getpid());
    int rc = 0;
    for(int i = 0; i < NUM_CORES; i++) {
        rc = fork();
    }


    if (rc < 0) {
        // fork failed; exit
        fprintf(stderr, "fork failed\n");
        exit(1);
    } else if (rc == 0) {
        // child (new process)
        Spin(10);
        printf("hello, I am child (pid:%d)\n", (int) getpid());
    } else {
        // parent goes down this path (original process)
        Spin(10);
        printf("hello, I am parent of %d (pid:%d)\n",
        rc, (int) getpid());
    }

    return 0;
}