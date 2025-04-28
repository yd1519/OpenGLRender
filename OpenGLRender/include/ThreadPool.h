#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <atomic>
#include <mutex>
#include <functional>
#include <iostream>
#include <memory>
#include <queue>
#include <thread>

namespace OpenGL {

class ThreadPool {
public:

	explicit ThreadPool(const size_t threadCnt = std::thread::hardware_concurrency()) 
	: threadCnt_(threadCnt), 
	threads_(new std::thread[threadCnt])/*thread对象没有关联任何线程*/ {
		createThreads();
	}

	~ThreadPool() {
		waitTasksFinish();
		running_ = false;
		joinThreads();
	}

	inline size_t getThreadCnt() const {
		return threadCnt_;
	}
	
	template<class F>
	void pushTask(const F& task) {
		{//减少锁的作用域，尽快释放锁
			const std::lock_guard<std::mutex> lock(mutex_);
			tasks_.push(std::function<void(size_t)>(task));
		}
		tasksCnt_++;
	}
	//将给的task的进行打包，存储在tasks_中。
	template<class F, class... A>
	void pushTask(const F& task, const A &...args) {
		pushTask([task, args...] {task(args...); });
	}

	//阻塞直到任务队列清空
	void waitTasksFinish() const {
		while (true) {
			if (!paused) {
				if (tasksCnt_ == 0) {
					break;
				}
			}
			else {
				if (taskRunningCnt() == 0) {
					break;
				}
			}
			std::this_thread::yield();
		}
	}

private:
	//创建所有工作线程并启动任务调度循环
	void createThreads() {
		for (size_t i = 0; i < threadCnt_; i++) {
			threads_[i] = std::thread(&ThreadPool::taskWorker, this, i);
		}
	}

	//等待所有线程结束
	void joinThreads() {
		for (size_t i = 0; i < threadCnt_; i++) {
			threads_[i].join();
		}
	}

	//取出任务
	bool popTask(std::function<void(size_t)>& task) {
		const std::lock_guard<std::mutex> lock(mutex_);
		if (tasks_.empty()) {
			return false;
		}
		else {
			task = std::move(tasks_.front());
			tasks_.pop();
			return true;
		}
	}

	size_t taskQueueCnt() const {
		const std::lock_guard<std::mutex> lock(mutex_);
		return tasks_.size();
	}

	size_t taskRunningCnt() const {
		return tasksCnt_ - taskQueueCnt();
	}

	//工作线程执行函数，
	void taskWorker(size_t threadId) {
		while (running_) {
			std::function<void(size_t)> task;
			if (!paused && popTask(task)) {
				task(threadId);
				tasksCnt_--;
			}
			else {
				std::this_thread::yield();// 让出CPU时间片
			}
		}
	}


public:
	std::atomic<bool> paused{ false };
private:

	mutable std::mutex mutex_;
	std::atomic<bool> running_{ true };
	
	std::unique_ptr<std::thread[]> threads_;
	std::atomic<size_t> threadCnt_{ 0 };

	std::queue<std::function<void(size_t)>> tasks_ = {};//未执行的
	std::atomic<size_t> tasksCnt_{ 0 };//未执行+执行中的
};


}
#endif
