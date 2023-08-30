#include "packet_manager.h"

#include <iostream>

#include "packet_id.h"
#include "error_code.h"

void PacketManager::Init(const int32_t max_user_cnt)
{
	user_manager_.Init(max_user_cnt);

	packet_handlers_[(int)PACKET_ID::kSYS_USER_CONNECT] = &PacketManager::ConnectHandler;
	packet_handlers_[(int)PACKET_ID::kSYS_USER_DISCONNECT] = &PacketManager::DisconnectHandler;

	packet_handlers_[(int)PACKET_ID::kLOGIN_REQUEST] = &PacketManager::LoginHandler;
}

void PacketManager::Run()
{
	packet_process_thread_ = std::thread([this]() { PacketProcessThread(); });
}

void PacketManager::Terminate()
{
	is_thread_running_ = false;

	if (DestroyPacketProcessThread() == false) {
		std::cout << "[DestroyPacketProcessThread] Failed to Destroy Process Thread\n";
	}
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
			ProcessPacket(*pkt);
		}

		if (auto pkt = DequeueSystemPacket(); pkt.has_value()) {
			idle = false;
			ProcessPacket(*pkt);
		}

		if (idle) {
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
	}
}

bool PacketManager::ProcessPacket(PacketInfo& pkt)
{
	if (packet_handlers_.contains(pkt.id_)) {
		(this->*(packet_handlers_[pkt.id_]))(pkt.session_index_, pkt.data_size_, pkt.data_);
		// TODO user ringbuffer movefront
	}

	return false;
}

bool PacketManager::DestroyPacketProcessThread()
{
	is_thread_running_ = false;

	if (packet_process_thread_.joinable())
	{
		packet_process_thread_.join();
	}

	std::cout << "[DestroyPacketProcessThread] Packet Process Thread Destroyed\n";
	return true;
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
	LOGIN_RESPONSE_PACKET loginResPacket;
	loginResPacket.PacketId = (uint16_t)PACKET_ID::kLOGIN_RESPONSE;
	loginResPacket.PacketLength = sizeof(LOGIN_RESPONSE_PACKET);

	// 접속자 수가 최대 인원을 초과했다면 실패
	if (user_manager_.GetCurrentUserCnt() >= user_manager_.GetMaxUserCnt())
	{
		loginResPacket.Result = (uint16_t)ERROR_CODE::kLOGIN_USER_USED_ALL_OBJ;
		SendPacketFunc(session_idx, (char*)&loginResPacket, sizeof(LOGIN_RESPONSE_PACKET));
		return;
	}

	// UserID 필드로 접속 여부 확인
	if (user_manager_.FindUserIndexByID(p_user_id) == -1)
	{
		user_manager_.AddUser(p_user_id, session_idx);

		loginResPacket.Result = (uint16_t)ERROR_CODE::kNONE;
		SendPacketFunc(session_idx, (char*)&loginResPacket, sizeof(LOGIN_RESPONSE_PACKET));
	}
	else
	{
		// 이미 접속중인 경우
		loginResPacket.Result = (uint16_t)ERROR_CODE::kLOGIN_USER_ALREADY;
		SendPacketFunc(session_idx, (char*)&loginResPacket, sizeof(LOGIN_RESPONSE_PACKET));
		return;
	}
}
