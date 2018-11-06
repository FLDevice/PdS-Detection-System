#pragma once

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <queue>
#include <utility>

template <typename T>
class BlockingQueue
{
	/** the queue */
	std::queue<T> queue;
	
	/** atomic boolean, is the queue still valid? */
	std::atomic<bool> is_valid = true;
	/** grants mutual exclusion on operations */
	std::mutex mutex;
	/** condition variable to sinchronize access */
	std::condition_variable condition;
	
public:
	/** Destructor */
	~BlockingQueue();

	/** Pop from the queue in a thread-safe way. Blocks execution until a value is available.
	Return true on success */
	bool waitPop(T& out);
	/** Push a new value into the queue. */
	void push(T value);
	/** Clear all items from the queue. */
	void clear();

	/** Returns whether or not this queue is valid. */
	bool isValid();
	/** Check whether or not the queue is empty. */
	bool empty();

	/** Invalidate the queue. Used to ensure no conditions are being waited on in waitPop when
	 a thread or the application is trying to exit. */
	void invalidate();
};
