#include <iostream>
#include <iomanip>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <cstring>
#include <unistd.h>
#include <map>
#include <deque>
#include <vector>
#include <algorithm>
#include <ctime>

using namespace std;

// Matching structures with producer
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

// Store last 5 prices for calculating average
struct CommodityData {
    double currentPrice;
    deque<double> priceHistory;
    double lastPrice;
    double lastAverage;
};

void clearScreen() {
    cout << "\033[2J\033[1;1H";
}

string getArrow(double current, double previous) {
    if (previous == 0) return " ";
    return (current > previous) ? "↑" : "↓";
}

void printDashboard(map<string, CommodityData>& commodities) {
    clearScreen();
    vector<string> commodityNames = {
        "ALUMINIUM", "COPPER", "COTTON", "CRUDEOIL", "GOLD",
        "LEAD", "MENTHAOIL", "NATURALGAS", "NICKEL", "SILVER", "ZINC"
    };

    cout << "+----------------------+------------+------------+" << endl;
    cout << "| Commodity           | Price      | AvgPrice   |" << endl;
    cout << "+----------------------+------------+------------+" << endl;

    for (const auto& name : commodityNames) {
        auto it = commodities.find(name);
        double price = 0.0;
        double avgPrice = 0.0;
        string priceArrow = " ";
        string avgArrow = " ";

        if (it != commodities.end()) {
            price = it->second.currentPrice;
            
            // Calculate average of up to 5 latest prices
            if (!it->second.priceHistory.empty()) {
                avgPrice = 0;
                for (double p : it->second.priceHistory) {
                    avgPrice += p;
                }
                avgPrice /= it->second.priceHistory.size();
            }
            
            priceArrow = getArrow(price, it->second.lastPrice);
            avgArrow = getArrow(avgPrice, it->second.lastAverage);
        }

        cout << "| " << left << setw(20) << name 
             << "| " << right << fixed << setprecision(2) << setw(8) << price << priceArrow 
             << " | " << setw(8) << avgPrice << avgArrow << " |" << endl;
    }
    
    cout << "+----------------------+------------+------------+" << endl;
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        cerr << "Usage: " << argv[0] << " <buffer_size>" << endl;
        return 1;
    }

    int buffer_size = atoi(argv[1]);
    
    // Get shared memory and semaphores
    key_t key = ftok("shmfile", 65);
    if (key == -1) {
        cerr << "Error generating key" << endl;
        return 1;
    }

    int shmid = shmget(key, buffer_size * sizeof(sharedBuffer) + sizeof(sharedMemory), 0666);
    if (shmid == -1) {
        cerr << "Error accessing shared memory" << endl;
        return 1;
    }

    sharedMemory* memory = (sharedMemory*)shmat(shmid, nullptr, 0);
    if (memory == (void*)-1) {
        cerr << "Error attaching to shared memory" << endl;
        return 1;
    }

    int semid = semget(key, 3, 0666);
    if (semid == -1) {
        cerr << "Error accessing semaphores" << endl;
        return 1;
    }

    struct sembuf waitM = {MUTEX, -1, 0};
    struct sembuf signalM = {MUTEX, 1, 0};
    struct sembuf waitF = {FULL, -1, 0};
    struct sembuf signalF = {FULL, 1, 0};
    struct sembuf waitE = {EMPTY, -1, 0};
    struct sembuf signalE = {EMPTY, 1, 0};

    map<string, CommodityData> commodities;
    
    // Main consumption loop
    while (true) {
        semop(semid, &waitF, 1);
        semop(semid, &waitM, 1);

        // Consume item
        sharedBuffer item = memory->buffer[memory->out];
        memory->out = (memory->out + 1) % buffer_size;

        // Update commodity data
        if (strlen(item.name) > 0) {
            auto& data = commodities[item.name];
            data.lastPrice = data.currentPrice;
            data.currentPrice = item.price;
            data.priceHistory.push_back(item.price);
            if (data.priceHistory.size() > 5) {
                data.priceHistory.pop_front();
            }
            double newAvg = 0;
            for (double p : data.priceHistory) {
                newAvg += p;
            }
            newAvg /= data.priceHistory.size();
            data.lastAverage = newAvg;
        }

        semop(semid, &signalM, 1);
        semop(semid, &signalE, 1);

        // Update dashboard
        printDashboard(commodities);
        
        // Small delay to prevent screen flicker
        usleep(100000); // 100ms delay
    }

    shmdt(memory);
    return 0;
}
