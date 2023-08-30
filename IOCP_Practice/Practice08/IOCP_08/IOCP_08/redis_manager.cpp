#include "redis_manager.h"

bool RedisManager::Init(std::string ip, uint16_t port)
{
	if (Connect(ip, port) == false) {
		std::cout << "[RedisManager] Connect Error!\n";
		return false;
	}

	task_handlers_[(int)REDIS_TASK_ID::kREQUEST_LOGIN] = &RedisManager::LoginHandler;

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

void RedisManager::LoginHandler(uint32_t session_idx, uint16_t data_size, char* p_data)
{
	auto p_login_pkt = (RedisLoginReq*)p_data;

	RedisLoginRes res_login_pkt;
	res_login_pkt.Result = (UINT16)ERROR_CODE::kLOGIN_USER_INVALID_PW;

	std::string value;
	// TODO : RedisClient에 맞게 수정
	//if (mConn.get(p_login_pkt->UserID, value))
	//{
	//	res_login_pkt.Result = (UINT16)ERROR_CODE::NONE;

	//	if (value.compare(p_login_pkt->UserPW) == 0)
	//	{
	//		res_login_pkt.Result = (UINT16)ERROR_CODE::NONE;
	//	}
	//}

	//RedisTask resTask;
	//resTask.UserIndex = task.UserIndex;
	//resTask.TaskID = RedisTaskID::RESPONSE_LOGIN;
	//resTask.DataSize = sizeof(RedisLoginRes);
	//resTask.pData = new char[resTask.DataSize];
	//CopyMemory(resTask.pData, (char*)&res_login_pkt, resTask.DataSize);

	//PushResponse(resTask);
}

bool RedisManager::Connect(std::string ip, uint16_t port)
{
	return redis_client_.Initialize(ip, port, 10, 1);
}

void RedisManager::ProcessTaskThread()
{
	while (is_running_)
	{
		auto task = GetTaskReq();
		if (task.has_value()) {
			ProcessTask(*task);
			continue;
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
}

bool RedisManager::ProcessTask(RedisTask task)
{
	auto task_id = static_cast<uint16_t>(task.TaskID);

	if (task_handlers_.contains(static_cast<uint16_t>(task_id))) {
		(this->*(task_handlers_[task_id]))(task.UserIndex, task.DataSize, task.pData);
		task.Release();
	}

	return false;
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
