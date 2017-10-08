#include	"threadpool.h"

#include	<thread>
#include	<condition_variable>
#include	<deque>
#include	<atomic>
#include	<vector>
#include	<algorithm>

#include	"compute.h"

// ----------------------------------------------------------------------------

namespace {

// ----------------------------------------------------------------------------

std::vector<std::thread> g_threads;

std::condition_variable g_cond;
std::mutex g_mutex;

std::condition_variable g_jobFinishedCondition;
std::mutex g_jobFinishedMutex;

using ThreadJobFinishedFunc = std::function<void()>;

struct Job
{
	Job(const ThreadPoolFunc& func = nullptr, const ThreadJobFinishedFunc& callback = nullptr)
		: func_(func)
		, finishedCallback_(callback)
	{
	}

	ThreadPoolFunc func_ = nullptr;
	ThreadPoolJob id_ = -1;
	ThreadJobFinishedFunc finishedCallback_ = nullptr;
};

std::deque<Job> g_jobs;
std::atomic<bool> g_stop = false;
std::atomic<int> g_activeJobs = 0;

std::thread::id g_mainThreadId;

// ----------------------------------------------------------------------------

void ThreadFunction()
{
	while (true)
	{
		Job job;
		{
			std::unique_lock<std::mutex> lock(g_mutex);
			while (!g_stop && g_jobs.empty())
			{
				g_cond.wait(lock);
			}

			if (g_stop)
			{
				return;
			}
			
			job = g_jobs.front();
			g_jobs.pop_front();
		}
		
		if (job.func_)
		{
			g_activeJobs++;

			job.func_();
			if (job.finishedCallback_)
			{
				job.finishedCallback_();	
			}

			g_activeJobs--;
		}

		{
			std::unique_lock<std::mutex> lock(g_jobFinishedMutex);
			g_jobFinishedCondition.notify_all();
		}
	}
}

// ----------------------------------------------------------------------------

}

// ----------------------------------------------------------------------------

void ThreadPool_Initialise(const int numThreads)
{
	g_mainThreadId = std::this_thread::get_id();

	for (int i = 0; i < numThreads; i++)
	{
		g_threads.push_back(std::thread(ThreadFunction));
	}	
}

// ----------------------------------------------------------------------------

ThreadPoolJob ThreadPool_ScheduleJob(const std::function<void()>& f)
{
	{
		std::unique_lock<std::mutex> lock(g_mutex);
		g_jobs.push_back(Job(f));
	}

	g_cond.notify_one();

	return 0;
}


// ----------------------------------------------------------------------------

void ThreadPool_WaitForJobs()
{
	while (true)
	{
		bool empty = false;
		{
			std::unique_lock<std::mutex> lock(g_mutex);
			empty = g_jobs.empty();
		}

		if (empty)
		{
			std::unique_lock<std::mutex> lock(g_jobFinishedMutex);
			g_jobFinishedCondition.notify_all();
			break;
		}
		else
		{
#if 1
			std::this_thread::yield();
#else
			std::unique_lock<std::mutex> lock(g_jobFinishedMutex);
			g_jobFinishedCondition.wait(lock);
#endif
		}
	}

	// TODO this may still have an occasional deadlock when doing CSG edits
	while (g_activeJobs > 0)
	{
		std::this_thread::yield();
//		std::unique_lock<std::mutex> lock(g_jobFinishedMutex);
//		g_jobFinishedCondition.wait(lock, [&]() { return g_activeJobs == 0; });
	}
}

// ----------------------------------------------------------------------------

void ThreadPool_Destroy()
{
	g_stop = true;

	{
		std::unique_lock<std::mutex> lock(g_mutex);
		g_cond.notify_all();
	}

	std::for_each(begin(g_threads), end(g_threads), [](std::thread& t)
	{
		t.join();
	});
}

// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------

void JobGroup::schedule(const ThreadPoolFunc& func)
{
	numScheduledJobs_++;

	{
		auto callback = std::bind(&JobGroup::onJobFinished, this);

		std::unique_lock<std::mutex> lock(g_mutex);
		g_jobs.push_back(Job(func, callback));
	}

	g_cond.notify_one();
}

// ----------------------------------------------------------------------------

void JobGroup::onJobFinished()
{
	numCompletedJobs_++;	
}

// ----------------------------------------------------------------------------

void JobGroup::wait()
{
	while (numCompletedJobs_ != numScheduledJobs_)
	{
		std::this_thread::yield();
	}
}

// ----------------------------------------------------------------------------



