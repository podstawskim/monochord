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
    struct timespec referece;
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
    printf("Signal parsing data (PID): %d\n", inputData.data_number );
    printf("Signal parsing commands (PID): %d\n", inputData.com_number);
    if(inputData.binary_file_name == NULL) {
        printf("Binary file wasn't specified!\n");
    } else {
        printf("Binary file: %s\n", inputData.binary_file_name);
    }
    if(inputData.text_file_name == NULL) {
        printf("Text file (default): stdout\n");
    }
    else
        printf("Text file: %s\n\n", inputData.text_file_name);
    printf("Rejestrator PID: %d\n", pid);
    
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
    printf("Signal with data recieved: %d\n", sig);

    memcpy(&signalData, &si->si_value.sival_int, sizeof(int)); //odbieranie danych
    data_signal_pid = si->si_pid;
    data_signal_appeared = 1;

    printf("Process (PID) %d, send data: %f\n", data_signal_pid, signalData);
}

static void com_sig_info_handler(int sig, siginfo_t* si, void *ucontext_t) {
    printf("Signal with commands recieved: %d\n", sig);
    commandData = si->si_value.sival_int;
    com_signal_pid = si->si_pid;
    com_signal_appeared = 1;

    printf("Process (PID) %d, send command: %d\n", com_signal_pid, commandData);
}

void serve_command_signal(struct write_data* writeData, struct registration_data* registrationData, struct input_data inputData) {

    
    //zaczynamy od sprawdzenia jakie opcje zostaly przekazane
    char command_str [MAX_COMMAND];
    sprintf(command_str, "%d", commandData);
    
    //przy przyjeciu nowego sygnalu sterujacego zeruje wartosci z poprzedniego chyba ze przyszedl 255 
    
    if(strcmp(command_str, "255") != 0) {
        registrationData->start = 0;
        registrationData->source_pid = 0;
        registrationData->reference_time = 0;
        registrationData->global_time = 0;
    }

    if(command_str[0] == '0') return; //nie zostala przeslana 1 rejestracja nie moze sie zaczac
    else if( command_str[0] == '1') {
        registrationData->start = 1; //mozna zaczac rejestracje
        printf("Registration is now starting!\n");

        //przepisuje oryginalny ciag komend do nowego
        char commands[MAX_COMMAND - 1];
        for(int i = 0; i < MAX_COMMAND; i++) {
            commands[i] = command_str[i+1];
        }

        //sprawdzenie pozostalych opcji i wykonanie konkretnych akcji dla nich
        if(strchr(commands, '0')) { //bez punkctu referencyjnego
            registrationData->global_time = 1;
        }
        if(strchr(commands, '1')) { //nowy punkt referencyjny (start rejestracji)
            clock_gettime(CLOCK_REALTIME, &writeData->referece);
            registrationData->reference_time = 1;
        }
        if(strchr(commands, '2')) {
            if(writeData->referece.tv_sec == 0 && writeData->referece.tv_nsec == 0) /* jak nie ma starego punktu to tworzony jest nowy, */
                  clock_gettime(CLOCK_REALTIME, &writeData->referece);                /* w przeciwnym wypadku uzywany jest stary */                                   
            registrationData->reference_time = 1;      
        }

        if(strchr(commands, '4')) {
            registrationData->source_pid = 1;
        }

        if(strchr(commands, '8')) {
            //sprawdzanie czy jest plikiem regularnym czy nie, jesli tak to jest czyszczony
            if(txt_file != stdout) { //isFileRegular(inputData.text_file_name)
                //jest regularny wiec trzeba go wyczyscic czyli zamykamy, bo aktualnie jest otwarty i otwieramy od nowa
                fclose(txt_file);
                txt_file = open_txt_file(inputData.text_file_name);
                printf("Content of %s, has been removed!\n", inputData.text_file_name);
                //powinno byc wyczysczone
            } else {
                perror("File is not regular file!\n");
            }        
        }
    } else if (strcmp(command_str, "255") == 0) {
        //odyslanie sygnalu do nadawcy 
        /* 4 bity:
            0 - rejestracja działa,
            1 - używany jest punkt referencyjny, 
            2 - używana jest identyfikacja źródeł, 
            3 - używany jest format binarny. */

            send_info_signal(writeData, registrationData, inputData);
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
        //obliczanie czasu referencyjnego
        writeData->signal_time = calculate_time(writeData);
        //zapisywanie w innym formacie niz w przypadku czasu bez punktu referencyjnego
        float mili_secs = writeData->signal_time.tv_nsec/1000000;
        struct tm now;
        localtime_r(&writeData->signal_time.tv_sec, &now);
        if(sprintf(writeData->time, "%02d:%02d:%02d.%.0f ", now.tm_hour, now.tm_min, now.tm_sec, mili_secs) < 0) {
            perror("sprintf time error!\n");
            exit(1);
        }
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

struct timespec calculate_time(struct write_data* writeData) {
    struct timespec deltaTime = {0};
    struct timespec currentTime = {0};
    clock_gettime(CLOCK_MONOTONIC, &currentTime);
    deltaTime.tv_sec = currentTime.tv_sec - writeData->referece.tv_sec;
    deltaTime.tv_nsec = currentTime.tv_sec - writeData->referece.tv_nsec;

    if(deltaTime.tv_nsec < 0) {
        deltaTime.tv_sec -= 1;
        deltaTime.tv_nsec = 1000000000 + deltaTime.tv_nsec;
    }

    return deltaTime;
}


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

    printf("inserting into text file/stdout! \n");
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

    printf("inserting into binary file!\n");
    fwrite(&writeData->signal_time, sizeof(struct timespec), 1, bin_file ); //zpisuje strukture timespec
    fflush(bin_file);
    fwrite(&writeData->data, sizeof(float), 1, bin_file);
    fflush(bin_file);
    fwrite(&writeData->source, sizeof(pid_t), 1, bin_file);
    fflush(bin_file);

    printf("data was inserted into binary file!\n");
    fclose(bin_file);

}

// int isFileRegular( char* file_name ) {
//     struct stat statbuf;
//     stat(file_name, &statbuf);
//     if((statbuf.st_mode & S_IFMT) == S_IFREG)
//         return 1;
//     else
//         return 0; //nie jest reg
// }

void send_info_signal(struct write_data* writeData, struct registration_data* registrationData, struct input_data inputData) {

    printf("\nSending signal with information!\n");
    unsigned int signal_value = 0; //informacja do przeslania
    if(inputData.binary_file_name != NULL) signal_value |= 1UL << 0;
    if(registrationData->source_pid) signal_value |= 1UL << 1;
    if(registrationData->reference_time) signal_value |= 1UL << 2;
    if(registrationData->start) signal_value |= 1UL << 3;

    printf("information bits: %d%d%d%d\n\n", ((signal_value>>0)&1U),((signal_value>>1)&1U),((signal_value>>2)&1U),((signal_value>>3)&1U));

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