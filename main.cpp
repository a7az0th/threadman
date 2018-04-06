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
	B(int* arr): buff(arr) {}
	virtual void body(int index, int threadIdx, int numThreads) override {
		buff[index] = threadIdx;
		printf("Thread %d processing index %d\n", threadIdx, index);
	}
private:
	int *buff;
};



int main() {

	const int numThreads = getProcessorCount();

	ThreadManager threadman;

	A example;
	example.run(threadman, numThreads);

	int arr[50000];
	B b(arr);
	b.run(threadman, 5000, numThreads);
	return 0;
}