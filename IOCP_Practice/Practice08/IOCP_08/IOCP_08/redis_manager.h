#pragma once

#include <cstdint>
#include <string>
#include <concurrent_queue.h>
#include <vector>
#include <thread>
#include <optional>

#include "redis_task.h"

#include "RedisClient.hpp"

class RedisManager
{
public:
	bool Init(std::string ip, uint16_t port = 6379);
	void Run(const int32_t max_thread_cnt);
	void Terminate();

	void PushTaskReq(RedisTask task);
	std::optional<RedisTask> GetTaskRes();

private:
	void LoginHandler(uint32_t session_idx, uint16_t data_size, char* p_data);

	bool Connect(std::string ip, uint16_t port);
	void ProcessTaskThread();
	bool ProcessTask(RedisTask task);
	std::optional<RedisTask> GetTaskReq();
	void PushTaskRes(RedisTask task);

	CRedisClient redis_client_;

	bool			is_running_ = false;
	std::thread		task_process_thread_;

	typedef void(RedisManager::* REDIS_TASK_FUNCTION)(uint32_t, uint16_t, char*);
	std::unordered_map<uint16_t, REDIS_TASK_FUNCTION> task_handlers_;

	std::vector<std::thread> worker_list_;
	Concurrency::concurrent_queue<RedisTask> task_req_queue_;
	Concurrency::concurrent_queue<RedisTask> task_res_queue_;
};