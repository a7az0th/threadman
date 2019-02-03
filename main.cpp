#include "stdio.h"

#include "threadman.h"
#include "timer.h"

using namespace a7az0th;

struct A : MultiThreaded {
	void threadProc(int index, int numThreads) override {
		buff[index] = index;
		printf("Thread %d of %d running\n", index, numThreads);
	}
private:
	int buff[64];
};

struct B : MultiThreadedFor {
	B(int* arr, int size): buff(arr) {
		memset(buff, 0, sizeof(int)*size);
	}
	virtual void body(int index, int threadIdx, int numThreads) override {
		buff[threadIdx]++;
	}
private:
	int *buff;
};



int main() {

	const int numThreads = getProcessorCount();

	ThreadManager threadman;

	//A example;
	//example.run(threadman, numThreads);

	int arr[50];
	B b(arr,50);
	b.run(threadman, 50000, numThreads);
	for (int i = 0 ; i < numThreads; i++) {
		printf("Thread %d processed %d elements\n", i, arr[i]);
	}
	return 0;
}
