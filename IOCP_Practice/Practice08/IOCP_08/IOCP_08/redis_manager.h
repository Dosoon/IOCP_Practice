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
	bool Connect(std::string ip, uint16_t port);
	void ProcessTask();
	std::optional<RedisTask> GetTaskReq();
	void PushTaskRes(RedisTask task);

	CRedisClient redis_client_;

	bool	is_running_ = false;

	std::vector<std::thread> worker_list_;
	Concurrency::concurrent_queue<RedisTask> task_req_queue_;
	Concurrency::concurrent_queue<RedisTask> task_res_queue_;
};