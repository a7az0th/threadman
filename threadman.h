#pragma once

#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <assert.h>

namespace a7az0th {

	// How many CPUs we support
	const int MAX_CPU_COUNT = 64;


	// Return the number of processors available on the system
	static int getProcessorCount(void) {
		const int cpu_count = std::thread::hardware_concurrency();
		return (cpu_count < 2) ? 1 : cpu_count;
	}

	// A simple mutex. Used by threads to lock a section of code so that only the locker thread can execute the code
	// When the mutex is locked all threads arriving at the location are stopped until the working thread unlocks it
	class Mutex {
		std::mutex csect;
		Mutex(const Mutex& rhs) = delete; // non-copyable class...
		Mutex& operator = (const Mutex& rhs) = delete; // ... disallow evil constructors
	public:
		Mutex() {}
		~Mutex() {}
		void enter(void) { csect.lock(); }
		void leave(void) { csect.unlock(); }
	};

	// A simple blocking event used in inter-thread comminication
	// Used to signal the waiting threads that a condition has been met
	class Event {
		std::condition_variable c;
		std::mutex m;
		Event(const Event& rhs) = delete; // non-copyable class...
		Event& operator = (const Event& rhs) = delete; // ... disallow evil constructors
	public:
		Event(void) {}
		~Event(void) {}
		// Start waiting on the condition
		void wait(void) {
			std::unique_lock<std::mutex> lk(m);
			c.wait(lk);
			lk.unlock();
		}
		// Release one waiting thread
		void signal(void) {
			std::unique_lock<std::mutex> lk(m);
			c.notify_one();
			lk.unlock();
		}
		// Release all waiting threads
		void signalAll(void) {
			std::unique_lock<std::mutex> lk(m);
			c.notify_all();
			lk.unlock();

		}
	};


	struct ThreadManager;

	struct MultiThreaded {
		virtual ~MultiThreaded() {}

		// This does the actual work. It will be called by every thread spawned
		// @param index The index of the current worker thread, 0..numThreads-1;
		// @param numThreads The total number of workers.
		virtual void threadProc(int index, int numThreads) = 0;

		// Call this to run the code on the desired number of threads
		void run(ThreadManager& threadman, int numThreads);
	};

	struct MultiThreadedFor : MultiThreaded {
	public:
		virtual ~MultiThreadedFor() {}

		void run(ThreadManager& threadman, int numIterations, int numThreads) {
			idx = 0;
			count = numIterations;
			MultiThreaded::run(threadman, numThreads);
		}
		// This does the actual work. It will be for every index in count
		// @param index The index of the current worker thread, 0..numThreads-1;
		// @param numThreads The total number of workers.
		virtual void body(int index, int threadIdx, int numThreads) = 0;

	private:
		std::atomic<int> idx; // Atomic counter keeping track of the current index
		int count;            // How many times the thread procedure should be called

		void threadProc(int index, int numThreads) final {
			int i = 0;
			while ((i = idx++) < count) {
				body(i, index, numThreads);
			}
		}
	};

	// A generic thread manager. Responsible for creating, managing, scheduling and deallocating threads.
	struct ThreadManager {
	private:
		// The possible states a thread can be in
		enum ThreadState {
			THREAD_INIT = 100,
			THREAD_IDLE,
			THREAD_RUNNING,
			THREAD_DONE,
			THREAD_DEAD
		};

		// Internal struct for "boss"/"worker" synchronization:
		struct ThreadInfoStruct {
			int index;                  // Index of the current thread
			int numThreads;             // Total number of threads
			Event changedState;         // Signalled when the thread changes state
			Event* releaseMainThread;   // Threadman event. Used by the last thread to signal and unlock the ThreadMan
			std::thread handle;         // Handle to the actual thread object
			volatile ThreadState state; // The state of the current thread.
			MultiThreaded *algorithm;   // The algorithm the thread is going to execute
			std::atomic<int>* counter;  // A pointer to the atomic active thread counter. The threadman gets signalled when this reaches zero
		} info[MAX_CPU_COUNT];

		int threadsInPool;        // Number of threads currently inside the threadpool
		std::atomic<int> counter; // An atomic counter. Determines the number of currently working threads. Used to signal the main thread when all work is done
		Event waitForThreads;     // A wait condifion. The threadmanager waits on this while the threads are working.


		// Spawned threads enter here.
		// When a thread comes here it will enter idle state and wait for the thread manager to release it.
		// When the thread manager spawns all threads, it initializes their contexts and releases them
		// At this point all threads unpause and call the thread procedure given by the user.
		// The main thread is released when all spawned threads exit this method.
		// @param info - The context of the thread spawned
		static void exec(ThreadInfoStruct *info) {
			// Are we done?
			bool done = false;
			std::atomic<int>& cnt = *info->counter;
			do {
				// Set the current thread state to IDLE
				// The thread manager needs to initialize rest of the thread data
				// Before we can proceed with the execution
				info->state = THREAD_IDLE;

				// Wait for the thread to be woken from the thread manager
				info->changedState.wait();

				// The thread has been awoken!
				// When the thread manager wakes a thread, it will set its state to RUNNING
				switch (info->state) {
				case THREAD_RUNNING: {
					// If we have a job, do it
					MultiThreaded *job = info->algorithm;

					if (job) {
						job->threadProc(info->index, info->numThreads);
					}

					// After the job is done, decrease the global counter
					// keeping track of the threads that are currently running.
					if (0 == --cnt) {
						// If this is the last threads
						// Signal the thread manager that the main thread can continue.
						info->releaseMainThread->signal();
					}
					break;
				}

				case THREAD_DONE: {
					done = true;
					break;
				}
				default:
					break;
				}
			} while (!done);
			info->state = THREAD_DEAD;
		}

		// Used to add another thread to the threadpool
		void spawnNewThread(void) {
			// Get a context for the the next thread;
			ThreadInfoStruct& ti = info[threadsInPool];

			// Initialize the context
			ti.index = threadsInPool;               // Set the new thread's ID
			ti.state = THREAD_INIT;                 // Set initial thread state
			ti.releaseMainThread = &waitForThreads; // Give the workers a way to signal the main thread when they are done
			ti.algorithm = NULL;                    // Set the job to NULL (initially)
			ti.counter = &counter;

			// Run a thread with the context provided and get a pointer to it.
			ti.handle = std::thread(&exec, &ti);

			// Increment the number of currently active threads
			++threadsInPool;

			// Update the number of currently active threads throughout all thread contexts
			for (int i = threadsInPool - 1; i >= 0; i--) {
				info[i].numThreads = threadsInPool;
			}
		}

		static void wait(int ms) {
			std::this_thread::sleep_for(std::chrono::milliseconds(ms));
		}

		// Disallow evil constructors.
		ThreadManager(const ThreadManager& rhs) = delete;
		ThreadManager& operator = (const ThreadManager& rhs) = delete;
	public:
		ThreadManager() : threadsInPool(0), counter(0) {}
		~ThreadManager() { killall(); }

		// Run requested number of threads and wait for them to finish.
		// @param job The algorithm to run
		// @param numThreads How many threads to run the algorithm with.
		void run(MultiThreaded* job, int numThreads) {
			if (numThreads == 1) {
				job->threadProc(0, 1);
				return;
			}
			// Spawn all threads
			while (threadsInPool < numThreads) {
				spawnNewThread();
			}

			counter = numThreads;
			// For each thread
			for (int i = 0; i < numThreads; i++) {
				info[i].index = i;               // Set its index
				info[i].numThreads = numThreads; // Set total number of threads
				info[i].algorithm = job;         // Init the function that is going to be executed

				// Wait for the thread to enter its main loop before we let it run
				while (info[i].state != THREAD_IDLE) {
					wait(20);
				}

				// The thread has entered its main loop, so signal it to begin
				info[i].state = THREAD_RUNNING;
				info[i].changedState.signal();
			}

			// Wait for the last thread to signal and exit the wait loop.
			waitForThreads.wait();

			// round robin all threads until they come to rest
			while (1) {
				bool good = true;
				for (int i = 0; i < numThreads; i++) {
					if (info[i].state != THREAD_IDLE) {
						good = false;
					}
				}
				if (good) break;
				wait(20);
			}
		}

		// Stops all threads and frees the resources allocated by them
		void killall(void) {
			for (; threadsInPool > 0; threadsInPool--) {
				ThreadInfoStruct& threadInfo = info[threadsInPool - 1];
				while (threadInfo.state != THREAD_IDLE) wait(20);
				threadInfo.state = THREAD_DONE;
				threadInfo.changedState.signal();
				while (threadInfo.state != THREAD_DEAD) wait(20);
				threadInfo.handle.detach();
			}
		}
	};

	inline void MultiThreaded::run(ThreadManager& threadman, int numThreads) {
		threadman.run(this, numThreads);
	}

}//namespace a7az0th