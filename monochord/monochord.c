#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <poll.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <float.h>
#include <errno.h>
#include <limits.h>
#include <time.h>
#include <math.h>
#include <sys/timerfd.h>

#define MAXBUF 100


struct simulation_values {
    float amp;
    float freq;
    float probe; 
    float period;
    float time;
    int simulationState; //0-working 1-suspended 2-stopped
    int update;
}simulationValues;

struct reciever {
    short int pid;
    short int rt;
    int raport;
    float sig_data;

}recieverValues;

struct server_values {
    int sfd;
    int rv;
    char buffer[MAXBUF];
    struct addrinfo hints, *servinfo, *pointer;
    struct sockaddr_storage foreign_addr;
    socklen_t addr_len;
    struct sockaddr client_addr;
}serverValues;


void display_simulation_values();
void simulation();
void calculate_sin();
void set_starting_simulation_values();
int send_signal();
void get_data(char *);
void update_simulation_values(char*, char*);
float to_float(char*);
int to_int(const char*);
int check_RT_scope(short int);
void send_raport();
struct itimerspec parse_time(float);
struct pollfd *createPoll(int, int);

int main(int argc, char ** argv) {

   if(argc != 2) {
       perror("Port number was not specified!\n");
       exit(EXIT_FAILURE);
   }

    //pobieranie adresu portu przekazanego jako argument programu
   char* pEnd = NULL;
   if( (int)strtol(argv[1], &pEnd, 10) < 0) {    
       perror("Port number has to be above or equal 0!\n");
       exit(EXIT_FAILURE);
   } else {
       printf("Chosen port: %s \n", argv[1]);
   }

   //1. utworzenie i konfiguracja gniazda UDP (adres lokalny, port z parametru)
   set_starting_simulation_values(); 

   memset(&serverValues.hints, 0, sizeof(struct addrinfo));

   serverValues.hints.ai_flags = AI_PASSIVE; //for wildcard IP addr
   serverValues.hints.ai_socktype = SOCK_DGRAM; //datagram socket
   serverValues.hints.ai_family = AF_UNSPEC; //pozwala na IPv4/IPv6

   //pozyskiwianie adresu zeby go potem wykorzysrac w bind
   serverValues.rv = getaddrinfo(NULL, argv[1], &serverValues.hints, &serverValues.servinfo);
   if(serverValues.rv != 0) {
        fprintf(stderr, "Error while getaddrinfo: %s\n", gai_strerror(serverValues.rv));
        exit(EXIT_FAILURE);
    }

    //przechodzimy po wszystkich rezultatach aby wywolac bind kiedy sie bedzie dalo
    for(serverValues.pointer=serverValues.servinfo; serverValues.pointer!=NULL; serverValues.pointer=serverValues.pointer->ai_next) {
        if((serverValues.sfd = socket(AF_INET, SOCK_DGRAM, 0)) != -1) {
            if(bind(serverValues.sfd, serverValues.pointer->ai_addr, serverValues.pointer->ai_addrlen) != -1) {
                printf("Socket found and bounded!\n");
                break; //udalo się znaleźć
            } else {
                close(serverValues.sfd);
                perror("Bind error while listening!\n");
                continue;
            }
        } else {
            perror("Socket error while listening!\n");
            continue;
        }
    }

    //jesli doszlimsy az tutaj to znaczy ze albo nie udalo sie polaczyc albo 
    //jestesmy polaczeni, sprawdzamy czy pointer wskazuje na cokolwiek
    if(!serverValues.pointer) {
        perror("Socket could not be bounded!\n");
        exit(EXIT_FAILURE);
    }

    freeaddrinfo(serverValues.servinfo);
    //---------probe timer---------
    struct itimerspec probeTimeStructure = parse_time(1.0/simulationValues.probe);

    int probe_timer = timerfd_create(CLOCK_REALTIME, 0); 
    if(probe_timer == -1) {
        perror("Probe timer creation error!\n");
        exit(1);
    }
    if(timerfd_settime(probe_timer, 0, &probeTimeStructure, NULL) == -1) {
        perror("Error while setting time for probe timer!\n");
        exit(1);
    }

    //----------period timer-----------
    struct itimerspec periodTimerStructure;

    int period_timer = timerfd_create(CLOCK_REALTIME, 0);
    if(period_timer == -1) {
        perror("Error while creating period timer");
        exit(1);
    }

    struct itimerspec none = {{0, 0}, {0, 0}};
    struct itimerspec time_left = {{0, 0}, {0, 0}};

    struct pollfd *fds = createPoll(probe_timer, period_timer); 

    for(;;) {
        //zaczynam symulacje
        //symulacja będzie wstrzymana dopoki pid nie będzie prawidlowy
        
        // if( (getpgid(recieverValues.pid) != -1) && (simulationValues.period > 0) && (check_RT_scope(recieverValues.rt)) ) {
        //     set_reference_point(); //poczatek symulacji więc ustawiam punkt referencyjny
        //     simulation();
        // }

        int rtvalue = poll(fds, 3, -1);
        if(rtvalue < 0) {
            perror("Error while polling!\n");
            exit(1);
        }

        if((fds[0].revents & POLLIN) == POLLIN) {
            
            memset(serverValues.buffer, 0, sizeof(serverValues.buffer));
            serverValues.addr_len = sizeof serverValues.foreign_addr;
            int rv = recvfrom(serverValues.sfd, serverValues.buffer, MAXBUF -1, 0, &serverValues.client_addr, &serverValues.addr_len);
            if(rv == -1) 
            {
                fprintf(stderr, "Error while recvfrom!\n");
                exit(EXIT_FAILURE);
                
            } 
            int temp = simulationValues.probe; //pobieram aktualny do pozniejszej weryfikacji
            get_data(serverValues.buffer);

            if(recieverValues.raport) {
                send_raport();    
            }
            display_simulation_values();

            if(simulationValues.period >= 0 && simulationValues.update) {
                simulationValues.simulationState = 0; //working

                if(simulationValues.period > 0) {
                    periodTimerStructure = parse_time(simulationValues.period);
                    if(timerfd_settime(period_timer, 0, &periodTimerStructure, &none) == -1) 
                        exit(1);
                }
            }

            if(temp != simulationValues.probe) {
                probeTimeStructure = parse_time(1.0/simulationValues.probe);
                if(timerfd_settime(probe_timer, 0, &probeTimeStructure, &none) == 1)
                    exit(1);
            }   
        }

        if((fds[1].revents & POLLIN) == POLLIN) {
            read(probe_timer, NULL, sizeof(uint64_t));
            if(simulationValues.simulationState < 2 && simulationValues.period >= 0) {
                
                if( send_signal() == -1 && simulationValues.period >= 0) {
                    if(!simulationValues.simulationState) timerfd_settime(period_timer, 0, &none, &time_left);
                    simulationValues.simulationState = 1;
                }
            } else if( simulationValues.simulationState == 1 && simulationValues.period >= 0) {
                simulationValues.simulationState = 0;
                timerfd_settime(period_timer, 0, &time_left, &none);
            }
            //uruchamiam symulacje
            if(!simulationValues.simulationState) simulation();
        }

        if((fds[2].revents & POLLIN) == POLLIN) {
            read(period_timer, 0, sizeof(uint64_t));
            timerfd_settime(period_timer, 0, &none, &none);
            simulationValues.simulationState = 2;
        }
        
    } 
    close(probe_timer);
    close(period_timer);
    close(serverValues.sfd);
    return 0;  
}


void display_simulation_values() {

    printf("---------- SIMULATION VALUES ----------\n");
    printf("Amp: %f\n", simulationValues.amp );
    printf("Freq: %f\n", simulationValues.freq);
    printf("Probe: %f\n", simulationValues.probe);
    printf("Period: %f\n", simulationValues.period);
    printf("Reciever PID: %d\n", recieverValues.pid);
    printf("Real-Time signal: %d\n", recieverValues.rt); 
}

void simulation() {

    //while
    //sleep(1/simulationValues.probe);
    calculate_sin();
    

}
void calculate_sin() {

    float x = (2 * M_PI * simulationValues.time * simulationValues.freq); //! sprawdzic
    float y = simulationValues.amp * sinf(x); //sin w C zwraca tylko double, wiec uzywam sinf, dodatkowo flga -lm jest wymagana ptzy kompilacji
    recieverValues.sig_data = y;
    simulationValues.time += (1.0/ simulationValues.probe); 
    
}

void set_starting_simulation_values() {
    simulationValues.amp = 1.0;
    simulationValues.freq = 0.25;
    simulationValues.probe = 1.0;
    simulationValues.period = -1;
    recieverValues.pid = 1;
    recieverValues.rt = 0;
    recieverValues.raport = 0;
    simulationValues.time = 0.0;
    simulationValues.update = 0;
    simulationValues.simulationState = 2;
    recieverValues.sig_data = 0.0;
}

int send_signal() {
   
    union sigval sv;
    sv.sival_int = 0;

    memcpy(&sv.sival_int, &recieverValues.sig_data, sizeof(int)); //obsluga float
    if(!check_RT_scope(recieverValues.rt) && (getpgid(recieverValues.pid) == -1) ) {
        printf("Can't send signal to this reciever!\n");
        return -1;
    }

    if(sigqueue(recieverValues.pid, recieverValues.rt, sv) == -1) { 
        perror("Error while sendig signal with data!\n");
        return -1;
    } 
    return 0;
}

void get_data(char *buffer) {

    simulationValues.update = 0;
    char *pch = NULL;
    int firstIteration = 1;
    pch = strtok(buffer, " \t:\n");
    //if(!strcmp("raport", pch)) recieverValues.raport = 1;
    char *arg;
    char *val;
    do
    {
        if (firstIteration)
        {
            firstIteration=0;
            arg = pch;
            val = strtok(NULL, " \t:\n");
        }
        else
        {
            arg = strtok(NULL, " \t:\n");
            val = strtok(NULL, " \t:\n");
        }
        pch=val; //bez tego idziemy w nieskocznosc;
        update_simulation_values(arg, val);
    }while (pch != NULL);
}

void update_simulation_values(char *arg, char *val) {

    //printf("%s ", buffer);

    //sprawdzam czy jest null bo petla w get_data wykonuje sie o jeden raz za duzo

    if(arg == NULL && val == NULL) return;

    if(!strcmp("raport", arg)) {
        //tutaj bedzie zmienna wskazujaca czy raport by czy nie
        recieverValues.raport = 1; // trzeba wywolac raport

    }

    if(!strcmp("freq", arg)) {
        simulationValues.freq = to_float(val);
    }

    if(!strcmp("amp", arg)) {
        simulationValues.amp = to_float(val);
    }

    if(!strcmp("probe", arg)) {
        simulationValues.probe = to_float(val);
    }

    if(!strcmp("period", arg)) {
        simulationValues.update = 1;
        simulationValues.period = to_float(val);
    }

    if(!strcmp("pid", arg)) {
        recieverValues.pid = to_int(val);
    }

    if(!strcmp("rt", arg)) {
        recieverValues.rt = to_int(val);
    }
}

float to_float(char *value) {
    char *pEnd = NULL;
    errno = 0;
    float val = strtof(value, &pEnd);

    if ((errno == ERANGE && (val == FLT_MAX || val == FLT_MIN)) 
        || (errno != 0 && val == 0)) {
        perror("Error while to_float\n");
        exit(1);
    }

    if(pEnd == value) {
        perror("to_float didn't find any numbers!\n");
        exit(1);
    }

    return val;
}

int to_int(const char* buff)
{
	int val = 0;
	char* endptr = NULL;
	errno = 0;
	val = (int)strtol(buff, &endptr, 10);
	if ((errno == ERANGE && (val == INT_MAX || val == INT_MIN))
	    || (errno != 0 && val == 0)) {
		perror("Error while to_int!\n");
		exit(EXIT_FAILURE);
	}

	if (endptr == buff)
	{
		printf("to_int didn't find any numbers");
		exit(1);
	}
	return val;
}

int check_RT_scope(short int rt) {
    if(SIGRTMIN+rt <= SIGRTMAX)
        return 1;
    else {
        return 0;
    }
}

void send_raport() {

    //1. do bufora ktory zostanie wyslany trzeba wpisac aktualne ustawienie parametrow

    char *rt = malloc(7);
    if(check_RT_scope(recieverValues.rt)) {
        strcpy(rt, "correct");
    } else {
        strcpy(rt, "wrong");
    }

    char *pid = malloc(11);
    if(getpgid(recieverValues.pid) != -1) {
        strcpy(pid, "exists");
    } else {
        strcpy(pid, "not exists");
    }

    char *period = malloc(10);
    if(simulationValues.period == 0) {
        strcpy(period, "non-stop");
    } else if(simulationValues.period < 0){
        strcpy(period, "stopped");
    } else if(simulationValues.period > 0){
        strcpy(period, "suspended");
    }

    char *info_msg = malloc(MAXBUF+MAXBUF);
    //info_msg = malloc(MAXBUF);
    sprintf(info_msg, "\n---------------RAPORT---------------\namp: %f\nfreq: %f\nprobe: %f\nperiod: %f %s\npid: %d %s\nrt: %d %s\n", 
                        simulationValues.amp, simulationValues.freq, simulationValues.probe, 
                        simulationValues.period, period, recieverValues.pid, pid, recieverValues.rt, rt);


    
    //odsylanie wiadomosci
    if(sendto(serverValues.sfd, info_msg, strlen(info_msg), 0, &serverValues.client_addr, serverValues.addr_len ) == -1) {
        perror("Error while sending info msg!\n");
        exit(1);
    }
    recieverValues.raport = 0;


    //memset(info_msg, 0 , sizeof(MAXBUF));
}

struct itimerspec parse_time(float time_) {
    long sec = (long)time_;
    long nsec = (long)((time_-sec) * 1000000000);
    struct itimerspec temp = {{sec, nsec}, {sec, nsec}};
    return temp;
}

struct pollfd *createPoll(int probe_timer, int period_timer) {
    
    struct pollfd *fds;
    fds = calloc(3, sizeof(struct pollfd));
    fds[0].revents = 0;
    fds[0].fd = serverValues.sfd;
    fds[0].events = POLLIN;
   
    fds[1].events = POLLIN;
    fds[1].fd = probe_timer;
    fds[1].revents = 0;

    fds[2].fd = period_timer;
    fds[2].events = POLLIN;
    fds[2].revents = 0;

    return fds;
}