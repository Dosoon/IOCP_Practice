#include "redis_manager.h"

bool RedisManager::Init(std::string ip, uint16_t port)
{
	if (Connect(ip, port) == false) {
		std::cout << "[RedisManager] Connect Error!\n";
		return false;
	}

	std::cout << "[RedisManager] Connected to Redis Server\n";
	task_handlers_[(int)REDIS_TASK_ID::kREQUEST_LOGIN] = &RedisManager::LoginHandler;

	return true;
}

void RedisManager::Run(const int32_t max_thread_cnt)
{
	is_running_ = true;

	worker_list_.reserve(max_thread_cnt);

	for (auto i = 0; i < max_thread_cnt; i++)
	{
		worker_list_.emplace_back([this]() { ProcessTaskThread(); });
	}
}

bool RedisManager::Start(UserManager* user_manager, int32_t thread_cnt)
{
	p_ref_user_manager_ = user_manager;

	if (Init() == false) {
		return false;
	}

	Run(thread_cnt);

	return true;
}

void RedisManager::Terminate()
{
	is_running_ = false;

	for (auto& worker : worker_list_) {
		if (worker.joinable()) {
			worker.join();
		}
	}

	std::cout << "[DestroyThread] Redis TaskProcess Thread Destroyed\n";
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
	res_login_pkt.result_ = (UINT16)ERROR_CODE::kLOGIN_USER_INVALID_PW;

	std::string redis_pw;
	if (redis_client_.Get(p_login_pkt->user_id_, &redis_pw) == RC_SUCCESS) {

		if (redis_pw.compare(p_login_pkt->user_pw_) == 0) {
			p_ref_user_manager_->SetUserID(session_idx, p_login_pkt->user_id_);
			res_login_pkt.result_ = static_cast<uint16_t>(ERROR_CODE::kNONE);
		}
	}

	// 응답 전송
	RedisTask res_task;
	res_task.UserIndex = session_idx;
	res_task.TaskID = REDIS_TASK_ID::kRESPONSE_LOGIN;
	res_task.DataSize = sizeof(RedisLoginRes);
	res_task.pData = new char[res_task.DataSize];
	CopyMemory(res_task.pData, (char*)&res_login_pkt, res_task.DataSize);

	PushTaskRes(res_task);
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
