#include	"volume.h"
#include	"threadpool.h"
#include	"render.h"
#include	"timer.h"
#include	"compute.h"
#include	"log.h"
#include	"resource.h"
#include	"camera.h"
#include	"double_buffer.h"
#include	"glm_hash.h"

#include	<functional>
#include	<future>
#include	<set>
#include	<atomic>
#include	<deque>
#include	<condition_variable>
#include	<unordered_set>
#include	<Remotery.h>

#include	<glm/glm.hpp>
#include	<glm/ext.hpp>
using glm::ivec3;
using glm::vec3;
using glm::vec4;
using glm::vec2;

// ----------------------------------------------------------------------------

const ivec3 ChunkMinForPosition(const ivec3& p)
{
	const unsigned int mask = ~(CLIPMAP_LEAF_SIZE - 1);
	return ivec3(p.x & mask, p.y & mask, p.z & mask);
}
// ----------------------------------------------------------------------------

const ivec3 ChunkMinForPosition(const int x, const int y, const int z)
{
	const unsigned int mask = ~(CLIPMAP_LEAF_SIZE - 1);
	return ivec3(x & mask, y & mask, z & mask);
}

// ----------------------------------------------------------------------------

std::thread g_taskThread;
std::atomic<bool> g_taskThreadQuit = false;

typedef std::function<void(void)> TaskFunction;
enum TaskType { Task_None, Task_CSG, Task_UpdateLOD, Task_Ray };
struct Task
{
	Task() {}

	Task(const TaskType _type, const TaskFunction _func)
		: type(_type)
		, func(_func)
	{
	}

	TaskType		type = Task_None;
	TaskFunction	func = nullptr;
};

std::deque<Task> g_tasks;
std::mutex g_taskMutex;
std::condition_variable g_taskCondition;

void TaskThreadFunction()
{
	while (!g_taskThreadQuit)
	{
		Task task;
		std::deque<Task> tasksCopy;
		{
			std::lock_guard<std::mutex> lock(g_taskMutex);
			
			tasksCopy = g_tasks;

			if (!g_tasks.empty())
			{
				task = g_tasks.front();
				g_tasks.pop_front();
			}
		}

		// TODO there are a lot of casts dominating the queue when dragging
		if (false && !tasksCopy.empty())
		{
			int countCSG = 0, countUpdate = 0, countRay = 0;
			for (int i = 0; i < tasksCopy.size(); i++)
			{
				switch (tasksCopy[i].type)
				{
				case Task_CSG: countCSG++; break;
				case Task_UpdateLOD: countUpdate++; break;
				case Task_Ray: countRay++; break;
				}
			}

			printf("TaskQueue: CSG=%d update=%d ray=%d\n", countCSG, countUpdate, countRay);
		}

		if (task.func)
		{
			task.func();
		}
		else
		{
			std::unique_lock<std::mutex> lock(g_taskMutex);
			g_taskCondition.wait(lock);
		}
	}
}

// ----------------------------------------------------------------------------

void ScheduleTask(TaskType type, TaskFunction task)
{
	std::lock_guard<std::mutex> lock(g_taskMutex);
	g_tasks.push_back(Task(type, task));
	g_taskCondition.notify_one();
}

// ----------------------------------------------------------------------------

void Volume::initialise(const ivec3& cameraPosition, const AABB& worldBounds)
{
	clipmap_.initialise(worldBounds);

	g_taskThreadQuit = false;
	g_taskThread = std::thread(TaskThreadFunction);
}

// ----------------------------------------------------------------------------

void Volume::destroy()
{
	{
		std::lock_guard<std::mutex> lock(g_taskMutex);
		g_taskThreadQuit = true;
		g_tasks.clear();
	}

	g_taskCondition.notify_one();
	g_taskThread.join();

	clipmap_.clear();

	Compute_ClearCSGOperations();
}

// ----------------------------------------------------------------------------

void Volume::processCSGOperations()
{
	ScheduleTask(Task_CSG, std::bind(&Clipmap::processCSGOperations, clipmap_));
}

// ----------------------------------------------------------------------------

void Volume::applyCSGOperation(
	const vec3& origin, 
	const glm::vec3& brushSize, 
	const RenderShape brushShape,
	const int brushMaterial, 
	const bool rotateToCamera,
	const bool isAddOperation)
{
	clipmap_.queueCSGOperation(origin, brushSize, brushShape, brushMaterial, isAddOperation);
}

// ----------------------------------------------------------------------------

void Volume::Task_UpdateChunkLOD(const glm::vec3 currentPos, const Frustum frustum)
{
	Timer timer;
	timer.start();
	clipmap_.update(currentPos, frustum);
}

// ----------------------------------------------------------------------------

void Volume::updateChunkLOD(const vec3& currentPos, const Frustum& frustum)
{
	clipmap_.updateRenderState();

	ScheduleTask(Task_UpdateLOD,
		std::bind(&Volume::Task_UpdateChunkLOD, this, currentPos, frustum));
}

// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------