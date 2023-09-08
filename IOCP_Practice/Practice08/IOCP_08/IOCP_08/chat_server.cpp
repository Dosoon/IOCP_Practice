#include "chat_server.h"

#include <iostream>
#include <mutex>

#include "packet_id.h"

/// <summary> <para>
/// 정의해둔 콜백함수를 네트워크 클래스에 세팅하고, </para> <para>
/// 로직 스레드 생성 후 네트워크를 시작한다. </para>
/// </summary>
bool ChatServer::Start(uint16_t port, int32_t max_session_cnt, int32_t session_buf_size, int32_t redis_thread_cnt)
{
	is_server_running_ = true;

	SetDelegate();

	if (!network_.Start(port, max_session_cnt, session_buf_size)) {
		std::cout << "[StartServer] Failed to Start\n";
		return false;
	}

	packet_manager_.Start(max_session_cnt, redis_thread_cnt,
						  &p_ref_room_manager_, &p_ref_user_manager_, &p_ref_redis_manager_);

	std::cout << "[StartServer] Server Started\n";
	return true;
}

/// <summary>
/// 네트워크 클래스와 패킷 매니저를 종료시킨 후, 서버를 종료한다.
/// </summary>
void ChatServer::Terminate()
{
	network_.Terminate();
	packet_manager_.Terminate();
	is_server_running_ = false;
}

/// <summary>
/// 네트워크 단에서 사용될 Delegate 함수들 세팅
/// </summary>
void ChatServer::SetDelegate()
{
	// 네트워크에서 Accept, Recv, Disconnect 발생 시 에코 서버에서 수행할 Delegate 세팅
	network_.SetOnConnect(std::bind(&ChatServer::OnConnect, this, std::placeholders::_1));
	network_.SetOnRecv(std::bind(&ChatServer::OnRecv, this,
					   std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	network_.SetOnDisconnect(std::bind(&ChatServer::OnDisconnect, this, std::placeholders::_1));

	// 패킷 매니저에서 사용될 SendPacket 함수 세팅
	packet_manager_.SetSendPacket(std::bind(&Network::SendPacket, &network_,
								  std::placeholders::_1, std::placeholders::_2,
								  std::placeholders::_3));

	// 룸 매니저에서 사용될 SendPacket 함수 세팅
	p_ref_room_manager_.SetSendPacket(std::bind(&Network::SendPacket, &network_,
											 std::placeholders::_1, std::placeholders::_2,
											 std::placeholders::_3));
}

/// <summary>
/// 네트워크 단에서 Accept시에 수행됨
/// </summary>
void ChatServer::OnConnect(int32_t session_idx)
{
	// 패킷 매니저에 시스템 패킷 전달
	PacketInfo pkt { 
		.session_index_ = static_cast<uint16_t>(session_idx),
		.id_ = static_cast<int32_t>(PACKET_ID::kSYS_USER_CONNECT), 
		.data_size_ = 0,
		.data_ = nullptr
	};

	packet_manager_.EnqueueSystemPacket(pkt);
}

/// <summary>
/// 네트워크 단에서 Recv시에 수행됨
/// </summary>
void ChatServer::OnRecv(int32_t session_idx, const char* p_data, DWORD len)
{
	// 패킷 매니저에 전달
	packet_manager_.EnqueuePacket(session_idx, p_data, len);
}

/// <summary>
/// 네트워크 단에서 세션 Disconnect시에 수행됨
/// </summary>
void ChatServer::OnDisconnect(int32_t session_idx)
{
	// 패킷 매니저에 시스템 패킷 전달
	PacketInfo pkt{
		.session_index_ = static_cast<uint16_t>(session_idx),
		.id_ = static_cast<int32_t>(PACKET_ID::kSYS_USER_DISCONNECT),
		.data_size_ = 0,
		.data_ = nullptr
	};

	packet_manager_.EnqueueSystemPacket(pkt);
}