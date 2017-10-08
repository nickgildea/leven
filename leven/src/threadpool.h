#ifndef		THREADPOOL_H_HAS_BEEN_INCLUDED
#define		THREADPOOL_H_HAS_BEEN_INCLUDED

#include	<functional>
#include	<vector>
#include	<atomic>

using ThreadPoolFunc = std::function<void()>;
using ThreadPoolJob = int;

void ThreadPool_Initialise(const int numThreads);
void ThreadPool_Destroy();

ThreadPoolJob ThreadPool_ScheduleJob(const ThreadPoolFunc& f);
void ThreadPool_WaitForJobs();
void ThreadPool_WaitForJob(const ThreadPoolJob job);

class JobGroup
{
public:

	void schedule(const ThreadPoolFunc& f);
	void wait();

private:

	void onJobFinished();

	std::atomic<int> numScheduledJobs_ = 0;
	std::atomic<int> numCompletedJobs_ = 0;
};

#endif //	THREADPOOL_H_HAS_BEEN_INCLUDED

