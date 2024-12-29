#include <iostream>
#include <stdio.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <sys/sem.h>
#include <semaphore.h>
#include <cstring>
#include <random>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <unistd.h>

using namespace std;

struct sharedBuffer {
    char name[10];
    double price;
};
struct sharedMemory {
	int in;
	int out;
	int flag;
	sharedBuffer buffer[];
};

#define MUTEX 0
#define FULL 1
#define EMPTY 2

void printBuffer(sharedMemory* memory, int buffer_size) {
    string red="\033[31m";
    cout << "\nCurrent Buffer Contents:" << endl;
    for (int i = 0; i < buffer_size; i++) {
        if (strlen(memory->buffer[i].name) > 0) { // Check if slot is filled
            cout << "[" << i << "] " << memory->buffer[i].name << ": " 
                 << fixed << setprecision(2) << memory->buffer[i].price << endl;
        } else {
            cout << "[" << i << "] <empty>" << endl;
        }
    }
}

void print(const string& name, double price, int flag) {
    struct timespec tp;
    struct tm* timeinfo;
    string red="\033[31m";

    clock_gettime(CLOCK_REALTIME, &tp);
    timeinfo = localtime(&tp.tv_sec);

    char buffer[30];
    strftime(buffer, sizeof(buffer), "[%m/%d/%Y %H:%M:%S", timeinfo);
    long milliseconds = tp.tv_nsec / 1000000;

    std::ostringstream oss;
    oss << buffer << ":" << std::setfill('0') << std::setw(3) << milliseconds << "] ";

    if (flag == 0) {
        cout << oss.str() << red << name << ": generating a new value " << price << endl;
    } else if (flag == 1) {
        cout << oss.str() << red << name << ": trying to get mutex on shared buffer" << endl;
    } else if (flag == 2) {
        cout << oss.str() << red << name << ": placing " << price << " on shared buffer" << endl;
    } else if (flag == 3) {
        cout << oss.str() << red << "Sleeping for " << price << " ms" << endl;
    }
}

void clean() {
    key_t key = ftok("shmfile", 65);
    if (key == -1) {
        cerr << "Error generating key" << endl;
        return;
    }

    // Get the shared memory ID
    int shmid = shmget(key, 0, 0666);
    if (shmid == -1) {
        cerr << "Shared memory not found" << endl;
        return;
    }

    // Detach the shared memory
    sharedMemory* memory = (sharedMemory*)shmat(shmid, nullptr, 0);
    if (memory == (void*)-1) {
        cerr << "Error attaching to shared memory" << endl;
        return;
    }

    // Remove the shared memory
    if (shmctl(shmid, IPC_RMID, nullptr) == -1) {
        cerr << "Error removing shared memory" << endl;
    } else {
        cout << "Shared memory removed successfully" << endl;
    }

    // Get semaphore ID
    int semid = semget(key, 3, 0666);
    if (semid != -1) {
        // Remove the semaphores
        if (semctl(semid, 0, IPC_RMID) == -1) {
            cerr << "Error removing semaphores" << endl;
        } else {
            cout << "Semaphores removed successfully" << endl;
        }
    } else {
        cerr << "Error accessing semaphores" << endl;
    }

    // Detach from shared memory
    shmdt(memory);
}

int main(int argc, char* argv[]) {
    if (argc == 2 && strcmp(argv[1], "clean") == 0) {
        clean();
        return 0; // Exit after cleaning
    }
    if (argc != 6) {
        cerr << "You must send 6 parameters" << endl;
        return 1;
    }

    // Parse arguments
    string commodity_name = argv[1];
    double commodity_price_mean = atof(argv[2]);
    double commodity_price_sd = atof(argv[3]);
    int sleep_interval = atoi(argv[4]);
    int buffer_size = atoi(argv[5]);

    // Create shared memory
    key_t key = ftok("shmfile", 65);
    if (key == -1) {
        cerr << "Error in generating unique key" << endl;
        return 1;
    }

    int shmid = shmget(key, buffer_size*sizeof(sharedBuffer)+sizeof(sharedMemory) , 0666 | IPC_CREAT);
    if(shmid==-1)
    {
    	shmid=shmget(key, buffer_size*sizeof(sharedBuffer)+sizeof(sharedMemory) , 0666);
    	if (shmid < 0) {
        	cerr << "Error creating shared memory" << endl;
        	return 1;
    	}
    }
    

    sharedMemory* memory = (sharedMemory*)shmat(shmid, nullptr, 0);
    if (memory == (void*)-1) {
        cerr << "Error attaching to shared memory" << endl;
        return 1;
    }
    //flag=memory->in;
    //(semctl(semget(key,3,0666),MUTEX,GETVAL)==1)
    //{
    	//memory->in=0;
    	//memory->out=0;
    //}

    int semid = semget(key, 3, 0666 | IPC_CREAT);
    if(semid == -1)
    {
    	semid=semget(key,3,0666);
    	if (semid < 0) {
        	cerr << "Error creating semaphores" << endl;
        	return 1;
    	}
    }
    //int size = buffer_size-abs((memory->in)-(memory->out));
    if(memory->flag==0){
    	if (semctl(semid, MUTEX, SETVAL, 1) == -1 || semctl(semid, FULL, SETVAL, 0) == -1 ||
        semctl(semid, EMPTY, SETVAL, buffer_size) == -1) {
        cerr << "Error initializing semaphores" << endl;
        return 1;
    }
    	memory->flag=1;
    }
    

    struct sembuf waitM = {MUTEX, -1, 0};
    struct sembuf signalM = {MUTEX, 1, 0};

    struct sembuf waitF = {FULL, -1, 0};
    struct sembuf signalF = {FULL, 1, 0};

    struct sembuf waitE = {EMPTY, -1, 0};
    struct sembuf signalE = {EMPTY, 1, 0};

    default_random_engine generator(time(nullptr));
    normal_distribution<double> distribution(commodity_price_mean, commodity_price_sd);

   
    while (true) {
        double price = distribution(generator);

        print(commodity_name, price, 0);
        semop(semid, &waitE, 1);

        print(commodity_name, 0, 1);
        semop(semid, &waitM, 1);
        print(commodity_name, price, 2);
        
        strcpy(memory->buffer[memory->in].name, commodity_name.c_str());
        memory->buffer[memory->in].price = price;
        cout << memory->in <<endl;
        memory->in = (memory->in + 1)%buffer_size;
        printBuffer(memory, buffer_size);
        
        semop(semid, &signalM, 1);
        semop(semid, &signalF, 1);

        print(commodity_name, sleep_interval, 3);
        usleep(sleep_interval * 1000);
    }

    shmdt(memory);
    return 0;
}
