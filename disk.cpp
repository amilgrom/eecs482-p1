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
multimap<int, int> queue;

//mutexes
mutex requesterCountMutex;
mutex queueMutex;
mutex stdOutMutex;


//cvs
cv servicerWait;
vector<cv*> requesterWait;
//cv requesterWait;

//functions
void scheduler(void* a);
void servicer(void *a);
void requester(void *a);
int incRequesterCount();
void decRequesterCount();
void requesterPrint(int requester, int track);
void servicerPrint(int requester, int track);
void printLine(string str);

int main(int argc, char **args) {
	maxQueueSize = atoi(args[1]);
	vector<char *> *fileNames = new vector<char *>();
	for (int i = 2; i < argc; i++) {
		fileNames->push_back(args[i]);
	}
	cpu::boot((thread_startfunc_t) scheduler, (void *) fileNames, 0);
//	printLine("MAIN FINISHED");
	return 0;
}

void scheduler(void* a) {
//	printLine("Scheduler started");
	vector<char *> *fileNames = (vector<char *> *)a;
	for (int i = 0; i < fileNames->size(); i++) {
		thread t1 ((thread_startfunc_t) requester, (void *) fileNames->at(i));
	}
	thread t2 ((thread_startfunc_t) servicer, (void *) 0);
//	printLine("Scheduler finished");
}

void servicer(void* a) {
//	printLine("Servicer started");
	int currentAddrs = 0;
	int requesterID = -1;
	multimap<int, int>::iterator smlr, lrgr;
	while (true) {
		queueMutex.lock();
		requesterCountMutex.lock();
		while ((queue.size() == 0) || ((queue.size() < requesterCount) && (queue.size() < maxQueueSize))) {
			requesterCountMutex.unlock();
			servicerWait.wait(queueMutex);
			requesterCountMutex.lock();
		}
		requesterCountMutex.unlock();
		//grab and remove next disk
		smlr = queue.lower_bound(currentAddrs);
		if (queue.size() == 1) {
			smlr = queue.begin();
//			printLine("Size Of One: " + to_string(smlr->first));
			requesterID = smlr->second;
			queue.erase(smlr);
		}
		else if (smlr->first == currentAddrs) { //point to equivalent
//			printLine("Perfect Match: " + to_string(smlr->first));
			requesterID = smlr->second;
			queue.erase(smlr);
		}
		else {
			lrgr = smlr;
			int lrgrDiff = -1;
			int smlrDiff = -1;
			if (smlr == queue.begin()) {
				smlr = --queue.end();
				smlrDiff = 1000 - smlr->first + currentAddrs;
				lrgrDiff = lrgr->first - currentAddrs;
			}
			else if (smlr == queue.end()) {
				smlr--;
				smlrDiff = currentAddrs - smlr->first;
				lrgr = queue.begin();
				lrgrDiff = 1000 - currentAddrs + lrgr->first;
			}
			else {
				smlr--;
				smlrDiff = currentAddrs - smlr->first;
				lrgrDiff = lrgr->first - currentAddrs;
			}
//			printLine("currentTrack: " + to_string(currentAddrs));
//			printLine("lowerBound: " + to_string(smlr->first));
//			printLine("upperBound: " + to_string(lrgr->first));
//			printLine("smlrDiff: " + to_string(smlrDiff));
//			printLine("lrgrDiff: " + to_string(lrgrDiff));

			if (smlrDiff < lrgrDiff) {
				currentAddrs = smlr->first;
				requesterID = smlr->second;
				queue.erase(smlr);
			}
			else {
				currentAddrs = lrgr->first;
				requesterID = lrgr->second;
				queue.erase(lrgr);
			}
		}
		servicerPrint(requesterID, currentAddrs);
		requesterWait[requesterID]->signal();
//		requesterWait.broadcast();
		queueMutex.unlock();
	}
//	printLine("Servicer finished");
}

void requester(void* a) {
	int requesterID = incRequesterCount();
	cv *waiter = new cv;
	requesterWait.push_back(waiter);
//	printLine("Requester " + to_string(requesterID) + " started");
	char *fileName = (char *) a;
	string diskAddr = "";
	ifstream ifs(fileName);
	if (ifs) {
		while (getline(ifs, diskAddr)) {
			int addr = atoi(diskAddr.c_str());
			queueMutex.lock();
			while (queue.size() >= maxQueueSize) {
//				printLine("Requester " + to_string(requesterID) + " waiting on queue");
				requesterWait[requesterID]->wait(queueMutex);
//				requesterWait.wait(queueMutex);
			}
			queue.insert(pair<int, int>(addr, requesterID));
			requesterPrint(requesterID, addr);
			servicerWait.signal();
//			bool requestInQueue = true;
//			while (requestInQueue) {
				requesterWait[requesterID]->wait(queueMutex);
//				requesterWait.wait(queueMutex);
//				requestInQueue = false;
//				multimap<int, int>::iterator it = queue.lower_bound(addr);
//				while (!requestInQueue && (it->first == addr)) {
//					if (it->second == requesterID) requestInQueue = true;
//					else it++;
//				}
			}
			queueMutex.unlock();
		}
		ifs.close();
	}
	else {
		cerr << "Failed to open file: " << *fileName << endl;
	}
	decRequesterCount();
	servicerWait.signal();
//	printLine("requester " + to_string(requesterID) + " finished");
}

int incRequesterCount() {
	requesterCountMutex.lock();
	int ID = requesterCount++;
	requesterCountMutex.unlock();
	return ID;
}

void decRequesterCount() {
	requesterCountMutex.lock();
	requesterCount--;
	requesterCountMutex.unlock();
}

void requesterPrint(int requester, int track) {
	printLine("requester " + to_string(requester) + " track " + to_string(track));

}

void servicerPrint(int requester, int track) {
	printLine("service requester " + to_string(requester) + " track " + to_string(track));
}

void printLine(string str) {
	stdOutMutex.lock();
	cout << str << endl;
	stdOutMutex.unlock();
}
