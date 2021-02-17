#define _GNU_SOURCE

#include<signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <limits.h>
#include <time.h>
#include <dirent.h>
#include <getopt.h>
#include <string.h>
#include <errno.h>
#include <signal.h>

int toInt(const char*);
static void signalHandler(int, siginfo_t*, void*);
void interpretation(unsigned int);

int signalRecieved = 0;
unsigned int signal_value= 0;

int main(int argc, char** argv) {

    //zaczynamu od pobierania parametrow programu
    if(argc != 4) {
        perror("Wrong number of parameters where given!\n");
        exit(1);
    }

    //pobieranie parametrow getoptem
    int signal_number = 0;
    int pid = toInt(argv[3]);
    int opt_index = 0;
    while((opt_index=getopt(argc, argv, "c: :")) != -1) {
        switch (opt_index)
        {
        case 'c':
            signal_number = toInt(optarg);
            break;
        default:
            perror("Wrong parameter!\n");
            exit(1);
        }
    }

    printf("PID: %d, SIGNAL NUMBER: %d\n", pid, signal_number);

    const struct timespec sleepTime = {.tv_sec = 2, .tv_nsec =0};
    union sigval sv;
    sv.sival_int = 255;

    //wysylamy pod zadany adres pid wartosc 255 (do rejestratora)
    
    if(sigqueue(pid, signal_number, sv) == -1) { //! odkomentowac
        perror("Error while sending signal");
        exit(1);
    }

    //sigaction
    struct sigaction sa;
    sa.sa_sigaction = signalHandler;
    sa.sa_flags = SA_SIGINFO;

    if(sigaction(signal_number, &sa, NULL)) {
        perror("Error while sigaction!\n");
        exit(1);
    }

    nanosleep(&sleepTime, NULL);

    if(!signalRecieved) {
        printf("Signal with information datagram was not revieved!");
    }

    return 0;

}

int toInt(const char* buf) {

    int value = 0;
	char* pEnd = NULL;
	errno = 0;
	value = (int)strtol(buf, &pEnd, 10);
	if ((errno == ERANGE && (value == INT_MAX || value == INT_MIN))
	    || (errno != 0 && value == 0)) {
		perror("Error while parsing argument to int!\n");
		exit(1);
	}

	if (pEnd == buf)
	{
		fprintf(stderr, "No digits were found\n");
		exit(1);
	}
	return value;
}

static void signalHandler(int signal, siginfo_t *sig_info, void *ucontext) {
    printf("Signal recieved: %d\n", signal);
    //pobieram bity wyslane przez sygnal
    signal_value = sig_info->si_value.sival_int;
    signalRecieved = 1; 
    interpretation(signal_value);   
}

void interpretation(unsigned int signal_value) {
    printf("Interpreted values: \n");
    int info = signal_value;

    if((info - 8) >= 0) {
        printf("Registration: OPEN\n");
        info -=8;
    } else {
        printf("Registration: CLOSED\n");
    }

    if((info - 4) >= 0) {
        printf("Referencial point: USED\n");
        info -= 4;
    } else printf("Referencial point: NOT USED\n");

    if((info - 2) >= 0) {
        printf("Source identification: USED\n");
        info -= 2;
    } else printf("Source identification: NOT USED\n");

    if((info - 1) >= 0) printf("Binary format: USED\n");
    else printf("Binary format: NOT USED\n");
    // for(int i = 0; i<=3; i++) {
    //     switch (i)
    //     {
    //     case 3:
    //         if((signal_value >> i) & 1U) printf("Registration is open!\n");
    //         else printf("Registration is closed!\n");
    //         break;
    //     case 2:
    //         if((signal_value >> i) & 1U) printf("Referencial point is used!\n");
    //         else printf("Referencial pointf is not used!\n");
    //         break;
    //     case 1:
    //         if((signal_value >> i) & 1U) printf("Source identification is used!\n");
    //         else printf("Source identification is not being used!\n");
    //         break;
    //     case 0: 
    //         if((signal_value >> i) & 1U) printf("Binary format is being used!\n");
    //         else printf("Binary format is not being used!\n");
    //         break;
        
    //     default:
    //         printf("Switch error while interpretation!\n");
    //         break;
    //     }
    // }
}