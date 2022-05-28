#include <vector>
#include <thread>
#include <string>
#include <deque>
#include <chrono>
#include <iostream>

#include "task.h"


unsigned int Client::gen_id = 1;

long Fact(long n) {
    if (n == 0)
        return 1;
    else
        return n * Fact(n - 1);
}

// Client place //

Client::Client(int avgItems) {
    timeQueue = 0;
    timeCashbox = 0;
    items = rand() % (2 * avgItems);
    id = gen_id++;
}

unsigned int Client::GetId() {
    return id;
}

int Client::GetQueueTime() {
    return timeQueue;
}

int Client::GetCashboxTime() {
    return timeCashbox;
}

void Client::EnterQueue() {
    tin = chrono::system_clock::now();
}

void Client::ExitQueue() {
    timeQueue = chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - tin).count();
}

void Client::ExitCashbox(int time) {
    timeCashbox = time;
}

int Client::GetItems() {
    return items;
}

// Shop place //

Shop::Shop(int cashboxCount, int requestRate, double servicespeed, int avgItems, int maxCount) {
    cashbox = cashboxCount;
    this->requestRate = requestRate;
    this->servicespeed = servicespeed;
    this->avgItems = avgItems;
    maxqueue = maxCount;

    completedCount = 0;
    rejectedCount = 0;

    queueLen = 0;
    countProc = 0;

    cboxes = vector<Cashbox>();
    queue = deque<Client *>();

    for (int i = 0; i < cashbox; i++) {
        cboxes.push_back(Cashbox());
        cboxes[i].thrd = thread(&Shop::Start, this, i);
    }
}

void Shop::Start(int index) {
    while (completedCount + rejectedCount < requestRate) {
        mu.lock();
        queueLen += queue.size();
        countProc++;
        if (queue.size() > 0) {
            queue.at(0)->ExitQueue();
            int items = queue.at(0)->GetItems();
            unsigned int id = queue.at(0)->GetId();
            cboxes[index].workStat += items * servicespeed;
            completedCount++;
            queue.at(0)->ExitCashbox(items * servicespeed);
            queue.pop_front();
            mu.unlock();
            while (items > 0) {
                this_thread::sleep_for(chrono::milliseconds(servicespeed));
                items -= 1;
            }
            cout << "Клиент с ID " << id << " обслужен." << endl;
        } else {
            cboxes[index].waitStat += 10;
            mu.unlock();
            this_thread::sleep_for(chrono::milliseconds(10));
        }
    }
}

std::string Shop::Modelling() {
    srand(time(0));

    string out = "";
    int countClients = 0;
    std::vector<Client> clients;

    for (int i = 0; i < requestRate; i++) {
        clients.push_back(Client(avgItems));
    }

    double avgQueueLen = 0.0;

    while (countClients < requestRate) {
        int time = rand() % 1000;
        this_thread::sleep_for(std::chrono::milliseconds(time));
        mu.lock();
        avgQueueLen += queue.size();
        if (queue.size() < maxqueue) {
            queue.push_back(&clients.at(countClients));
            clients.at(countClients).EnterQueue();
            mu.unlock();
        } else {
            mu.unlock();
            rejectedCount++;
            std::cout << "Клиент с ID " << clients.at(countClients).GetId() << " не обслужен." << std::endl;
        }
        countClients++;
    }

    double avgWait = 0.0;
    double avgWork = 0.0;
    double avgTimeQueue = 0.0;
    double avgTimeCashBox = 0.0;
    double workTime = 0.0;

    for (int i = 0; i < cashbox; i++) {
        cboxes[i].thrd.join();
        avgWait += cboxes[i].waitStat;
        avgWork += cboxes[i].workStat;
    }
    for (int i = 0; i < requestRate; i++) {
        if (clients.at(i).GetQueueTime() == 0 && clients.at(i).GetCashboxTime() == 0)
            continue;
        avgTimeQueue += clients.at(i).GetQueueTime();
        avgTimeCashBox += clients.at(i).GetCashboxTime();
    }

    avgQueueLen /= (double)countClients;
    workTime = avgWork + avgWait;
    avgWait /= (double)cashbox;
    avgWork /= (double)cashbox;
    avgTimeQueue /= (double)completedCount;
    avgTimeCashBox /= (double)completedCount;

    double lambda = requestRate * 1000.0 / avgWork;
    double m = 1000.0 / avgTimeCashBox;
    double r = lambda / m;
    double P0 = 1.0;
    for (int i = 1; i <= cashbox; i++)
        P0 += pow(r, i) / Fact(i);
    for (int i = cashbox + 1; i < cashbox + maxqueue; i++)
        P0 += pow(r, i) / (Fact(cashbox) * pow(cashbox, i - cashbox));

    P0 = 1.0 / P0;
    double Prej = pow(r, cashbox + maxqueue) * P0 / (double)(pow(cashbox, maxqueue) * Fact(cashbox));
    double Q = 1.0 - Prej;
    double A = lambda * Q;

    out += "\n\nНакопленная статистика\n";

    out += "\nОбслуженных покупателей: " + to_string(completedCount);
    out += "\nНеобслуженных покупателей: " + to_string(rejectedCount);
    out += "\nСредняя длина очереди: " + to_string(avgQueueLen);
    out += "\nСреднее время нахождение покупателя в очереди + на кассе: " + to_string(avgTimeQueue) + " + " + to_string(avgTimeCashBox);
    out += "\nСреднее время работы кассы: " + to_string(avgWork);
    out += "\nСреднее время простоя кассы: " + to_string(avgWait);

    out += "\nВероятность отказа: " + to_string((double)rejectedCount / (double)requestRate);
    out += "\nОтносительная пропускная способность магазина: " + to_string((double)completedCount / (double)requestRate);
    out += "\nАбсолютная пропускная способность: " + to_string(lambda * (double)completedCount / (double)requestRate);

    out += "\n\nТеоретические данные\n";

    out += "\nВероятность отказа: " + to_string(Prej);
    out += "\nОтносительная пропускная способность: " + to_string(Q);
    out += "\nАбсолютная пропускная способность: " + to_string(A);

    return out;
}
