#include "packet_manager.h"

#include <iostream>
#include <Windows.h>

#include "packet_id.h"
#include "error_code.h"
#include "redis_task.h"

void PacketManager::Init(const int32_t max_user_cnt, const int32_t redis_thread_cnt)
{
	user_manager_.Init(max_user_cnt);

	packet_handlers_[(int)PACKET_ID::kSYS_USER_CONNECT] = &PacketManager::ConnectHandler;
	packet_handlers_[(int)PACKET_ID::kSYS_USER_DISCONNECT] = &PacketManager::DisconnectHandler;

	packet_handlers_[(int)PACKET_ID::kLOGIN_REQUEST] = &PacketManager::LoginHandler;
	packet_handlers_[(int)REDIS_TASK_ID::kRESPONSE_LOGIN] = &PacketManager::LoginDBResHandler;

	if (redis_manager_.Init("127.0.0.1") == false) {
		return;
	}
	redis_manager_.Run(redis_thread_cnt);
}

void PacketManager::Run()
{
	packet_process_thread_ = std::thread([this]() { PacketProcessThread(); });
}

void PacketManager::Terminate()
{
	redis_manager_.Terminate();

	is_thread_running_ = false;
	DestroyPacketProcessThread();
}

bool PacketManager::EnqueuePacket(int32_t session_index, const char* p_data, DWORD len)
{
	auto user = user_manager_.GetUserByIndex(session_index);
	if (user == nullptr) {
		return false;
	}

	user->EnqueuePacketData(p_data, len);
	incoming_packet_user_index_queue_.push(session_index);

	return true;
}

void PacketManager::EnqueueSystemPacket(const PacketInfo& packet)
{
	system_packet_queue_.push(packet);
}

void PacketManager::SetSendPacket(std::function<void(uint32_t, char*, uint16_t)> send_packet)
{
	SendPacketFunc = send_packet;
}

void PacketManager::PacketProcessThread()
{
	is_thread_running_ = true;

	while (is_thread_running_)
	{
		bool idle = true;

		if (auto pkt = DequeuePacket(); pkt.has_value()) {
			idle = false;
			ProcessPacket(*pkt, true);
		}

		if (auto pkt = DequeueSystemPacket(); pkt.has_value()) {
			idle = false;
			ProcessPacket(*pkt);
		}

		if (auto task = redis_manager_.GetTaskRes(); task.has_value()) {
			idle = false;
			ProcessPacket(reinterpret_cast<PacketInfo&>(*task));
			task->Release();
		}

		if (idle) {
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
	}
}

bool PacketManager::ProcessPacket(PacketInfo& pkt, bool user_pkt)
{
	if (packet_handlers_.contains(pkt.id_)) {
		(this->*(packet_handlers_[pkt.id_]))(pkt.session_index_, pkt.data_size_, pkt.data_);

		if (user_pkt) {
			user_manager_.CompleteProcess(pkt.session_index_, pkt.data_size_);
		}
		return true;
	}

	return false;
}

void PacketManager::DestroyPacketProcessThread()
{
	is_thread_running_ = false;

	if (packet_process_thread_.joinable())
	{
		packet_process_thread_.join();
	}
}

std::optional<PacketInfo> PacketManager::DequeuePacket()
{
	if (int32_t user_idx; incoming_packet_user_index_queue_.try_pop(user_idx)) {

		auto user = user_manager_.GetUserByIndex(user_idx);
		if (user == nullptr) {
			return {};
		}

		auto pkt = user->GetPacketData();
		if (pkt.has_value() == false) {
			return {};
		}

		return *pkt;
	}

	return {};
}

std::optional<PacketInfo> PacketManager::DequeueSystemPacket()
{
	if (PacketInfo pkt; system_packet_queue_.try_pop(pkt)) {
		return pkt;
	}

	return {};
}

void PacketManager::ConnectHandler(uint32_t session_idx, uint16_t data_size, char* p_data)
{
	std::cout << "Connect Handler\n";
	auto user = user_manager_.GetUserByIndex(session_idx);

	if (user == nullptr) {
		std::cout << "[Error] ConnectHandler :: Invalid User Index\n";
		return;
	}
	
	user->Clear();
}

void PacketManager::DisconnectHandler(uint32_t session_idx, uint16_t data_size, char* p_data)
{
	std::cout << "Disonnect Handler\n";
	auto user = user_manager_.GetUserByIndex(session_idx);

	if (user == nullptr) {
		std::cout << "[Error] DisconnectHandler :: Invalid User Index\n";
		return;
	}

	if (user->GetDomainState() != User::DOMAIN_STATE::kNONE)
	{
		user_manager_.DeleteUserInfo(user->GetUserID());
	}
}

void PacketManager::LoginHandler(uint32_t session_idx, uint16_t data_size, char* p_data)
{
	std::cout << "Login Handler\n";

	auto p_login_pkt = reinterpret_cast<LOGIN_REQUEST_PACKET*>(p_data);
	auto p_user_id = p_login_pkt->UserID;

	// 응답 패킷
	LOGIN_RESPONSE_PACKET res_login_pkt;
	res_login_pkt.PacketId = (uint16_t)PACKET_ID::kLOGIN_RESPONSE;
	res_login_pkt.PacketLength = sizeof(LOGIN_RESPONSE_PACKET);

	// 접속자 수가 최대 인원을 초과했다면 실패
	if (user_manager_.GetCurrentUserCnt() >= user_manager_.GetMaxUserCnt())
	{
		res_login_pkt.Result = (uint16_t)ERROR_CODE::kLOGIN_USER_USED_ALL_OBJ;
		SendPacketFunc(session_idx, (char*)&res_login_pkt, sizeof(LOGIN_RESPONSE_PACKET));
		return;
	}

	// UserID 필드로 접속 여부 확인
	if (user_manager_.FindUserIndexByID(p_user_id) == -1)
	{
		RedisLoginReq login_db_pkt;
		CopyMemory(login_db_pkt.UserID, p_login_pkt->UserID, (kMAX_USER_ID_LEN + 1));
		CopyMemory(login_db_pkt.UserPW, p_login_pkt->UserPW, (kMAX_USER_PW_LEN + 1));

		RedisTask task;
		task.UserIndex = session_idx;
		task.TaskID = REDIS_TASK_ID::kREQUEST_LOGIN;
		task.DataSize = sizeof(RedisLoginReq);
		task.pData = new char[task.DataSize];
		CopyMemory(task.pData, reinterpret_cast<char*>(&login_db_pkt), task.DataSize);

		redis_manager_.PushTaskReq(task);
	}
	else
	{
		// 이미 접속중인 경우
		res_login_pkt.Result = (uint16_t)ERROR_CODE::kLOGIN_USER_ALREADY;
		SendPacketFunc(session_idx, (char*)&res_login_pkt, sizeof(LOGIN_RESPONSE_PACKET));
		return;
	}
}

void PacketManager::LoginDBResHandler(uint32_t session_idx, uint16_t data_size, char* p_data)
{
	std::cout << "LoginDBRes Handler\n";

	auto p_login_res_pkt = reinterpret_cast<RedisLoginRes*>(p_data);

	if (p_login_res_pkt->Result == (uint16_t)ERROR_CODE::kNONE) {
		auto user = user_manager_.GetUserByIndex(session_idx);
		user_manager_.AddUser(user->GetUserID(), session_idx);
		user->SetConfirmed(true);
	}

	// 응답 전송
	LOGIN_RESPONSE_PACKET login_res_pkt;
	login_res_pkt.PacketId = (uint16_t)PACKET_ID::kLOGIN_RESPONSE;
	login_res_pkt.PacketLength = sizeof(LOGIN_RESPONSE_PACKET);
	login_res_pkt.Result = p_login_res_pkt->Result;
	SendPacketFunc(session_idx, (char*)&login_res_pkt, sizeof(LOGIN_RESPONSE_PACKET));
}
