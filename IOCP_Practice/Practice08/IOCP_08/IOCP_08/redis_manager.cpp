#include "redis_manager.h"

bool RedisManager::Init(std::string ip, uint16_t port)
{
	if (Connect(ip, port) == false) {
		std::cout << "[RedisManager] Connect Error!\n";
		return false;
	}

	return true;
}

void RedisManager::Run(const int32_t max_thread_cnt)
{
	is_running_ = true;
}

void RedisManager::Terminate()
{
	is_running_ = false;

	for (auto& worker : worker_list_) {
		if (worker.joinable()) {
			worker.join();
		}
	}
}

void RedisManager::PushTaskReq(RedisTask task)
{
	task_req_queue_.push(task);
}

std::optional<RedisTask> RedisManager::GetTaskRes()
{
	RedisTask task;

	if (task_res_queue_.try_pop(task) == false) {
		return {};
	}

	return task;
}

bool RedisManager::Connect(std::string ip, uint16_t port)
{
	return redis_client_.Initialize(ip, port, 10, 1);
}

void RedisManager::ProcessTask()
{
	while (is_running_)
	{
		auto task = GetTaskReq();
		if (task.has_value()) {
			// TODO : Process
			continue;
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
}

std::optional<RedisTask> RedisManager::GetTaskReq()
{
	RedisTask task;

	if (task_req_queue_.try_pop(task) == false) {
		return {};
	}

	return task;
}

void RedisManager::PushTaskRes(RedisTask task)
{
	task_res_queue_.push(task);
}
