#include "BlockingQueue.h"

template <typename T>
BlockingQueue<T>::~BlockingQueue(){
	invalidate();
}

template <typename T>
bool BlockingQueue<T>::waitPop(T& out){
	std::unique_lock<std::mutex> lock(mutex);
	// block if data is not available
	m_condition.wait(lock, [this](){
		return !queue.empty() || !is_valid;
	});
	// exit if queue is not valid
	if(!is_valid){
		return false;
	}
	out = std::move(queue.front());
	queue.pop();
	return true;
}

template <typename T>
void BlockingQueue<T>::push(T value){
	std::lock_guard<std::mutex> lock(mutex);
	queue.push(std::move(value));
	condition.notify_one();
}

template <typename T>
void BlockingQueue<T>::clear(){
	std::lock_guard<std::mutex> lock(mutex);
	while(!queue.empty()){
		queue.pop();
	}
	condition.notify_all();
}

template <typename T>
bool BlockingQueue<T>::isValid(){
	std::lock_guard<std::mutex> lock(mutex);
	return is_valid;
}

template <typename T>
bool BlockingQueue<T>::empty(){
	std::lock_guard<std::mutex> lock(mutex);
	return queue.empty();
}

template <typename T>
void BlockingQueue<T>::invalidate(){
	std::lock_guard<std::mutex> lock(mutex);
	is_valid = false;
	condition.notify_all();
}