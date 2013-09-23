//
//  p1.cpp
//  
//
//  Created by Andrew Milgrom on 9/16/13.
//
//

#include <iostream>
#include <fstream>
#include <map>
#include <vector>
#include <string>
#include "thread.h"

using namespace std;

//shared resources
int requesterCount = 0;
int maxQueueSize = -1;
int* queue = NULL;
char** arguments = NULL;
int numFiles = 0;

//mutexes
mutex requesterCountMutex;
mutex queueMutex;
mutex stdOutMutex;


//cvs
cv servicerWait;
cv requesterWait;

//cv requesterWait;

//functions
void scheduler(void* a);
void servicer(void *a);
void requester(void *a);
void printLine(string str);

int main(int argc, char **args) {
	arguments = args;
	numFiles = argc - 2;
	maxQueueSize = atoi(args[1]);
	queue = new int[numFiles+1];
	for (int i = 0; i < numFiles; i++) {
		queue[i] = -1;
	}
	queue[numFiles] = 0;
	cpu::boot((thread_startfunc_t) scheduler, (void *) 0, 0);
	return 0;
}

void scheduler(void* a) {
//	printLine("Scheduler started");
	for (int i = 0; i < numFiles; i++) {
		thread t1 ((thread_startfunc_t) requester, (void *) 0);
	}
	thread t2 ((thread_startfunc_t) servicer, (void *) 0);
//	printLine("Scheduler finished");
}

void servicer(void* a) {
//	printLine("Servicer started");
	int currentAddrs = 0;
	bool firstRun = true;
	while (true) {
		queueMutex.lock();
		//check that queue is full
		requesterCountMutex.lock();
		while ((queue[numFiles] == 0) || ((queue[numFiles] < maxQueueSize) && (((firstRun == true) && (queue[numFiles] != numFiles)) || (queue[numFiles] < requesterCount)))) {
			requesterCountMutex.unlock();
//			printLine("Servicer waiting on queue");
			servicerWait.wait(queueMutex);
			requesterCountMutex.lock();
		}
		requesterCountMutex.unlock();
		firstRun = false;
		//grab and remove next address
		int index = 0;
		int smlDist = 10001;
		for (int i = 0; i < numFiles; i++) {
			if(queue[i] != -1) {
				int dist = abs(queue[i] - currentAddrs);
//				printLine("Requester " + to_string(i) + " dist " + to_string(dist));
				if (dist < smlDist) {
					smlDist = dist;
					index = i;
				}
			}
		}
		currentAddrs = queue[index];
		queue[index] = -1;
		queue[numFiles] -= 1;
		stdOutMutex.lock();
		cout << "service requester " << index << " track " << currentAddrs << endl;
		stdOutMutex.unlock();
		requesterWait.broadcast();
		queueMutex.unlock();
	}
//	printLine("Servicer finished");
}

void requester(void* a) {
	requesterCountMutex.lock();
	int requesterID = requesterCount++;
	requesterCountMutex.unlock();

//	printLine("Requester " + to_string(requesterID) + " started");
	string diskAddr = "";
	ifstream ifs(arguments[requesterID + 2]);
	if (ifs) {
		while (getline(ifs, diskAddr)) {
			int addr = atoi(diskAddr.c_str());
			queueMutex.lock();
			while (queue[numFiles] >= maxQueueSize) {
//				printLine("Requester " + to_string(requesterID) + " waiting on queue");
				requesterWait.wait(queueMutex);
			}
			queue[requesterID] = addr;
			queue[numFiles] += 1;
			stdOutMutex.lock();
			cout << "requester " << requesterID << " track " << addr << endl;
			stdOutMutex.unlock();
//			printLine("Queue Size: " + to_string(queue[numFiles]));

			servicerWait.signal();
			while (queue[requesterID] != -1) {
//				printLine("Requester " + to_string(requesterID) + " not serviced");
				requesterWait.wait(queueMutex);
			}
			queueMutex.unlock();
		}
		ifs.close();
	}
	else {
		cerr << "failed to open file" << arguments[requesterID + 2] << endl;
	}
	requesterCountMutex.lock();
	requesterCount--;
	requesterCountMutex.unlock();
	servicerWait.signal();
//	printLine("requester " + to_string(requesterID) + " finished");
}

void printLine(string str) {
	stdOutMutex.lock();
	cout << ">>>" << str << endl;
	stdOutMutex.unlock();
}
