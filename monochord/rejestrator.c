#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <time.h>
#include <signal.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>


//---------ZMIENNE---------

#define MAX_COMMAND 6
float signalData = 0;
int commandData = 0; //zeby zaczac rejestrowac sygnaly musi zaczynac sie od jedynki
pid_t data_signal_pid = 0;
pid_t com_signal_pid = 0; 

//sluza do sprawdzania czy dany sygnal się pokazal czy nie
int data_signal_appeared = 0;
int com_signal_appeared = 0;

FILE* bin_file = NULL;
FILE* txt_file = NULL;


struct input_data {
    char * binary_file_name; //domyslnie plik binarny nie bedzie stosowany
    char * text_file_name; //domyslnie uzywane jest standardowe wyjscie
    int data_number; //numer sygnalu przesylajacego dane
    int com_number; //numer syganly przesylajacego komendy
};

struct write_data {
    struct timespec signal_time;
    struct timespec* reference[100];
    int currentReference;
    char time[25];
    float data;
    pid_t source;
    
}writeData;

struct registration_data {
    int start;
    int global_time;
    int reference_time;
    int source_pid;
}registrationData;


//----------FUNKCJE------------

void print_arguments(struct input_data, pid_t);
FILE* open_txt_file(char*);
FILE* open_bin_file(char*);

//handler do obierania sygnalu przesylajacego dane
static void sig_info_handler(int, siginfo_t*, void *); //void* potencjalnie do usuniecia
//handler do odbierania sygnalu przesylajacego komendy
static void com_sig_info_handler (int, siginfo_t*, void*);

void serve_command_signal(struct write_data*, struct registration_data*, struct input_data);
void serve_data_signal(struct write_data*, struct registration_data*, struct input_data);
struct timespec calculate_time(struct write_data*);
void insert_data_into_txt(struct write_data*, struct input_data);
void insert_data_into_bin(struct write_data*, struct input_data);
int isFileRegular( char* );
void send_info_signal(struct write_data*, struct registration_data*, struct input_data);
int modify_bit(int, int, int);
void set_new_reference();
void restore_last_reference();
void format_reference_clock();
long get_elapsed_ms(struct timespec, struct timespec);
void ms_to_hours_min_sec(long);

int main(int argc, char ** argv) {

    char * pEnd = NULL;
    
    //1. odczytywanie parametrow przekazanych do programu 
    int opt_index = 0;
    
    //deklaracja struktury z danymi wejsciowymi
    struct input_data inputData;
    inputData.binary_file_name = NULL;
    inputData.text_file_name = NULL;
    inputData.data_number = 0;
    inputData.com_number = 0;
    writeData.currentReference = -1;
    
    //czytanie parametrow
    while((opt_index = getopt(argc, argv, "b:t:d:c:")) != -1 ) {
        switch(opt_index) {
            case 'b': 
                inputData.binary_file_name = optarg;
                break;
            case 't':
                if(strcmp(optarg, "-") == 0) 
                    txt_file = stdout;
                else
                    inputData.text_file_name = optarg; 
                break;
            case 'd': 
                inputData.data_number = (int)strtol(optarg, &pEnd, 10);
                break;
            case 'c': 
                inputData.com_number = (int)strtol(optarg, &pEnd, 10);
                break;
            default :
                printf("Incorrect option\n");
                return 1; //error   
        }
    }
    print_arguments(inputData, getpid());

    //2. otwieranie/ tworzenie plikow do zapisu

    //jesli danego pliku nie ma to zostanie 
    
    if(txt_file != stdout) { //zmiana na isRegularFile?
        txt_file = open_txt_file(inputData.text_file_name); // tworze/czyszcze
        fclose(txt_file);
    } 


    if(inputData.binary_file_name != NULL) {
        bin_file=open_bin_file(inputData.binary_file_name); //tworze/ czyszcze
        fclose(bin_file);
    }

    //3. otrzymywanie i interpretacja sygnalow
    struct sigaction sa_data;
    struct sigaction sa_command;
    
    //przyspisuje do sygnalow numery przekazane jako argumenty programu
    int data_singal = inputData.data_number;
    int command_signal = inputData.com_number;

    //ustawiam handlery i flagi dla obu typow sygnalu
    sa_data.sa_sigaction = sig_info_handler;
    sa_command.sa_sigaction = com_sig_info_handler;
    
    sa_data.sa_flags = SA_SIGINFO;
    sa_command.sa_flags = SA_SIGINFO;

    //wywoluje sigaction dla obu typow sygnalu
    if(sigaction(data_singal, &sa_data, NULL) != 0) {
        perror("Error while sigaction for data signal!\n");
        exit(1);
    }

    if(sigaction(command_signal, &sa_command, NULL) != 0) {
        perror("Error while sigaction for command signal!\n");
        exit(1);
    }

    //czekam na pojawienie sygnalow i gdy pojawi się dany typ wykonuje konkretne czynnosci
    while(1) {
        pause();
        if(data_signal_appeared) {
            serve_data_signal(&writeData, &registrationData, inputData);
            data_signal_appeared = 0;
        }
        if(com_signal_appeared) {
            serve_command_signal(&writeData, &registrationData, inputData);
            com_signal_appeared = 0;
        }
    }

    return 0;
}


void print_arguments(struct input_data inputData, pid_t pid) {

    printf("\n--------------------REJESTRATOR--------------------\n");
    printf("Rejestrator PID: %d\n", pid);
    printf("Data RT signal: %d\n", inputData.data_number );
    printf("Commands RT signal: %d\n", inputData.com_number);
    if(inputData.binary_file_name == NULL) {
        printf("Binary file wasn't specified!\n");
    } else {
        printf("Binary file: %s\n", inputData.binary_file_name);
    }
    if(inputData.text_file_name == NULL) {
        printf("Text file (default): stdout\n");
    }
    else
        printf("Text file: %s\n", inputData.text_file_name);
    
    
}

FILE* open_txt_file(char* text_file_name ) {
    FILE* f;
    f = fopen(text_file_name, "w");
    if(!f) {
        perror("There was some problem while opening text file!\n");
        return NULL;
    }
    
    return f;
}

FILE* open_bin_file(char* binary_file_name) {
    FILE* f;
    f = fopen(binary_file_name, "wb");
    if(!f) {
        perror("There was a problem while opening/creating binary file!\n");
        return NULL;
    }

    return f;
}

static void sig_info_handler(int sig, siginfo_t *si, void *ucontext) {
    printf("\nSignal with data recieved: %d\n", sig);
    memcpy(&signalData, &si->si_value.sival_int, sizeof(int)); //odbieranie danych
    data_signal_pid = si->si_pid;
    data_signal_appeared = 1;

    printf("Process (PID) %d, send data: %f\n", data_signal_pid, signalData);
}

static void com_sig_info_handler(int sig, siginfo_t* si, void *ucontext_t) {
    printf("\nSignal with commands recieved: %d\n", sig);
    commandData = si->si_value.sival_int;
    com_signal_pid = si->si_pid;
    com_signal_appeared = 1;

    printf("Process (PID) %d, send command: %d\n", com_signal_pid, commandData);
}

void serve_command_signal(struct write_data* writeData, struct registration_data* registrationData, struct input_data inputData) {

    int command = commandData;

    //zaczynamy od sprawdzenia czy jest 0
    if(command == 0) {
        registrationData->start = 0;
        printf("\nRegistration: CLOSED!\n");
    } else if( command == 255 ) {
        send_info_signal(writeData, registrationData, inputData);
    } else {
        int base = command - 1;
        registrationData->start = 1;
        printf("\nRegistration: OPEN!\n");

        if(base == 0) {
            registrationData->global_time = 1;
            registrationData->reference_time = 0;
        }
        
        if((base - 8) >= 0) {
            if(inputData.text_file_name) { //isFileRegular(inputData.text_file_name)
                //jest regularny wiec trzeba go wyczyscic czyli zamykamy, bo aktualnie jest otwarty i otwieramy od nowa
                fclose(fopen(inputData.text_file_name, "w"));
                printf("Content of %s, has been removed!\n", inputData.text_file_name);
                //powinno byc wyczysczone
            } else {
                perror("File is not regular file!\n");
            }
            base -=8;

        }
        if((base - 4 ) >= 0) {
            registrationData->source_pid = 1;
            base -= 4;
        }
        if((base - 2) >= 0) {
            // if(writeData->reference.tv_sec == 0 && writeData->reference.tv_nsec == 0) /* jak nie ma starego punktu to tworzony jest nowy, */
            //       clock_gettime(CLOCK_REALTIME, &writeData->reference);                /* w przeciwnym wypadku uzywany jest stary */                                   
            registrationData->reference_time = 1;
            registrationData->global_time = 0;
            restore_last_reference();
            base -= 2;

        }
        if((base - 1) >= 0) {

            // clock_gettime(CLOCK_REALTIME, &writeData->reference);
            registrationData->reference_time = 1;
            registrationData->global_time = 0;
            set_new_reference();
        }   
    }    
}

void serve_data_signal(struct write_data* writeData, struct registration_data* registrationData, struct input_data inputData) {


    if(registrationData->start == 0) {
        printf("registration remains closed!\n");
        return; //sprawdzam czy rejestracja jest otwarta
    }

    //skoro rejestracja jest otwarta to sprawdzam wszystkie opcje i zapisuje do pliku tekstowego dane
    
    //najpierw wpisywana jest data i godzina, sprawdzam czy uzywany jest czas referencyjny czy globalny
    if(registrationData->global_time == 1) { // to znaczy ze do pliku wpisujemy czas globalny
        
        struct timespec time;
        if(clock_gettime(CLOCK_REALTIME, &time)==-1) exit(1);
        float mili_secs = time.tv_nsec/1000000;
        struct tm now;
        localtime_r(&time.tv_sec, &now);
        if(sprintf(writeData->time, "%02d.%02d.%4d %02d:%02d:%02d.%.0f ",  now.tm_mday, now.tm_mon, now.tm_year + 1900, now.tm_hour, now.tm_min, now.tm_sec, mili_secs) < 0) {
            perror("sprintf time error!\n");
            exit(1);
        }
        
    } else { 
        format_reference_clock();
    }
    

    //teraz wpisujemy liczbe zapisaną z sygnalu, jeszcze w handlerze do niego 
    writeData->data = signalData; 

    //sprawdzam czy bedzie wspiywany pid do pliku
    if(registrationData->source_pid == 1) {
        writeData->source = data_signal_pid;
    } else {
        writeData->source = 0; //inaczej pidu nie bedzie
    }

    insert_data_into_txt(writeData, inputData);

    if(inputData.binary_file_name != NULL) {
        insert_data_into_bin(writeData, inputData);
    }
}

// struct timespec calculate_time(struct write_data* writeData) {
//     struct timespec deltaTime = {0};
//     struct timespec currentTime = {0};
//     clock_gettime(CLOCK_MONOTONIC, &currentTime);
//     deltaTime.tv_sec = currentTime.tv_sec - writeData->reference.tv_sec;
//     deltaTime.tv_nsec = currentTime.tv_sec - writeData->reference.tv_nsec;

//     if(deltaTime.tv_nsec < 0) {
//         deltaTime.tv_sec -= 1;
//         deltaTime.tv_nsec = 1000000000 + deltaTime.tv_nsec;
//     }

//     return deltaTime;
// }


void insert_data_into_txt(struct write_data* writeData, struct input_data inputData) {

    if(txt_file != stdout) { //zmiana na isRegularFile?
        //* jesli nie jest stdout to mozna sprawdzic czy jest regularny
        printf("txt file is beign used!");
        txt_file = fopen(inputData.text_file_name, "a+");
        if(!txt_file) {
            perror("Error while openeing file to add new record!\n");
            exit(1);
        }
    }

    printf("\n!!!inserting into text file/stdout!!! \n");
    fprintf(txt_file, "%s %f ", writeData->time, writeData->data);
    if(writeData->source != 0) fprintf(txt_file, "%d", writeData->source);
    fprintf(txt_file, "\n\n");
    
    if(txt_file != stdout) //zamykam tylko wtedy jak jest plikiem a nie stdout
        fclose(txt_file);   
}

void insert_data_into_bin(struct write_data *writeData, struct input_data inputData) {
    bin_file = fopen(inputData.binary_file_name, "ab");
    if(!bin_file) {
        perror("Error while opening binary file to write!\n");
        exit(1);
    }

    printf("\n!!!inserting data into binary file!!!\n");
    fwrite(&writeData->signal_time, sizeof(struct timespec), 1, bin_file ); //zpisuje strukture timespec
    fflush(bin_file);
    fwrite(&writeData->data, sizeof(float), 1, bin_file);
    fflush(bin_file);
    fwrite(&writeData->source, sizeof(pid_t), 1, bin_file);
    fflush(bin_file);

    
    fclose(bin_file);

}

void send_info_signal(struct write_data* writeData, struct registration_data* registrationData, struct input_data inputData) {

    printf("\n!!!Sending signal with information!!!\n");
    int signal_value = 0; //informacja do przeslania
    if(registrationData->start) signal_value += 8;
    if(registrationData->reference_time) signal_value += 4;
    if(registrationData->source_pid) signal_value +=2;
    if(inputData.binary_file_name != NULL) signal_value += 1;

    printf("Data for info-rejestrator: %d\n", signal_value);
    
    union sigval sv;
    sv.sival_int = signal_value;
    int info_signal = inputData.com_number;

    if(sigqueue(com_signal_pid, info_signal, sv) == -1) {
        perror("Sending signal with info unsuccessfull!\n");
        exit(1);
    }

    //printf("Signal with info should have arrived by now!\n");
}

int modify_bit(int value, int position, int b) {
    int mask = 1 << position;
    return (value & ~mask) | ((b << position) & mask);
}

void set_new_reference() {
    writeData.currentReference++;

    struct timespec* new_reference = malloc(sizeof(struct timespec));
    clock_gettime(CLOCK_MONOTONIC, new_reference);
    writeData.reference[writeData.currentReference] = new_reference;
}

struct timespec * get_current_reference() {
    if(writeData.currentReference >= 0) return writeData.reference[writeData.currentReference];
    else {
        printf("Reference time is not yet set!\n");
        return NULL;
    }
}

void restore_last_reference() {
    if(writeData.currentReference >= 0) {
        writeData.currentReference--;
    }
}

void format_reference_clock() {
    struct timespec now_time;
    clock_gettime(CLOCK_MONOTONIC, &now_time);
    struct timespec *reference_pointer = get_current_reference();

    if(reference_pointer != NULL) {
        long elapsed_ms = get_elapsed_ms(now_time, *reference_pointer);
        ms_to_hours_min_sec(elapsed_ms);
    }
}

long get_elapsed_ms(struct timespec after, struct timespec before) {
    return ((after.tv_sec - before.tv_sec) * 1000000000 + (after.tv_nsec - before.tv_nsec))/1000000;

} 

void ms_to_hours_min_sec(long ms) {
    long mili = ms;

    int hr = mili / 3600000;
    mili = mili - 3600000 * hr;
    int min = mili / 60000;
    mili = mili - 60000 * min;
    int sec = mili / 1000;
    mili = mili - 1000 * sec;

    sprintf(writeData.time, "%d:%d:%d.%ld", hr, min, sec, mili);
}