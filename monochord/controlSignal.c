#define _GNU_SOURCE
#include<signal.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <time.h>
#include <dirent.h>
#include <getopt.h>

int parseInt(const char* buff);

int main(int argc, char* argv[])
{
    int sig=5, numSigs, sigData;
    union sigval sv;
    sv.sival_int = 104;

    if (sigqueue(parseInt(argv[1]), sig, sv) == -1)
    {
        perror("Error in sigqueue");
        exit(EXIT_FAILURE);
    }
    exit(EXIT_SUCCESS);

}

int parseInt(const char* buff)
{
	int val = 0;
	char* endptr = NULL;
	errno = 0;
	val = (int)strtol(buff, &endptr, 10);
	if ((errno == ERANGE && (val == INT_MAX || val == INT_MIN))
	    || (errno != 0 && val == 0)) {
		perror("strtol");
		exit(EXIT_FAILURE);
	}

	if (endptr == buff)
	{
		fprintf(stderr, "No digits were found\n");
		exit(EXIT_FAILURE);
	}
	return val;
}