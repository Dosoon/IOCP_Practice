#include "packet_manager.h"

#include <iostream>
#include <Windows.h>

#include "packet_id.h"
#include "error_code.h"
#include "redis_task.h"
#include "packet_generator.h"

void PacketManager::Init(const int32_t max_user_cnt, const int32_t redis_thread_cnt)
{
	user_manager_.Init(max_user_cnt);

	BindHandler();

	redis_manager_.Start(&user_manager_, redis_thread_cnt);
}

void PacketManager::Run()
{
	packet_process_thread_ = std::thread([this]() { PacketProcessThread(); });
}

void PacketManager::Start(const int32_t max_user_cnt, const int32_t redis_thread_cnt)
{
	Init(max_user_cnt, redis_thread_cnt);
	Run();
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

/// <summary>
/// 각 핸들러에 대한 함수 포인터를 packet_handlers_ 맵에 설정
/// </summary>
void PacketManager::BindHandler()
{
	packet_handlers_[(int)PACKET_ID::kSYS_USER_CONNECT] = &PacketManager::ConnectHandler;
	packet_handlers_[(int)PACKET_ID::kSYS_USER_DISCONNECT] = &PacketManager::DisconnectHandler;

	packet_handlers_[(int)PACKET_ID::kLOGIN_REQUEST] = &PacketManager::LoginHandler;
	packet_handlers_[(int)REDIS_TASK_ID::kRESPONSE_LOGIN] = &PacketManager::LoginDBResHandler;
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

	std::cout << "[DestroyThread] PacketProcess Thread Destroyed\n";
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

	auto login_pkt = *reinterpret_cast<LOGIN_REQUEST_PACKET*>(p_data);
	auto p_user_id = login_pkt.UserID;

	// 응답 패킷
	auto res_login_pkt = SetPacketIdAndLen<LOGIN_RESPONSE_PACKET>(PACKET_ID::kLOGIN_RESPONSE);

	// 접속자 수가 최대 인원을 초과했다면 실패
	if (user_manager_.GetCurrentUserCnt() >= user_manager_.GetMaxUserCnt())
	{
		res_login_pkt.Result = (uint16_t)ERROR_CODE::kLOGIN_USER_USED_ALL_OBJ;
		SendPacketFunc(session_idx, (char*)&res_login_pkt, sizeof(res_login_pkt));
		return;
	}

	// UserID 필드로 접속 여부 확인
	if (user_manager_.FindUserIndexByID(p_user_id) == -1)
	{
		// Redis Task 생성 후 Redis 요청 전송
		RedisLoginReq login_db_task_body;
		CopyMemory(login_db_task_body.UserID, login_pkt.UserID, (kMAX_USER_ID_LEN + 1));
		CopyMemory(login_db_task_body.UserPW, login_pkt.UserPW, (kMAX_USER_PW_LEN + 1));

		auto task = SetTaskBody(session_idx, REDIS_TASK_ID::kREQUEST_LOGIN, login_db_task_body);
		redis_manager_.PushTaskReq(task);
	}
	else
	{
		// 이미 접속중인 경우
		res_login_pkt.Result = (uint16_t)ERROR_CODE::kLOGIN_USER_ALREADY;
		SendPacketFunc(session_idx, (char*)&res_login_pkt, sizeof(res_login_pkt));
	}
}

void PacketManager::LoginDBResHandler(uint32_t session_idx, uint16_t data_size, char* p_data)
{
	std::cout << "LoginDBRes Handler\n";

	auto login_db_res_pkt = *reinterpret_cast<RedisLoginRes*>(p_data);

	if (login_db_res_pkt.Result == (uint16_t)ERROR_CODE::kNONE) {
		user_manager_.SetUserLogin(session_idx);
	}

	// 응답 생성 및 전송
	auto login_res_pkt = SetPacketIdAndLen<LOGIN_RESPONSE_PACKET>(PACKET_ID::kLOGIN_RESPONSE);
	login_res_pkt.Result = login_db_res_pkt.Result;

	SendPacketFunc(session_idx, (char*)&login_res_pkt, sizeof(LOGIN_RESPONSE_PACKET));
}
