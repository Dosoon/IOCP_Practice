#pragma once
#include <deque>
#include <mutex>

#include "packet.h"
#include "network.h"
#include "packet_manager.h"

class ChatServer
{
public:
	bool Start(uint16_t port, int32_t max_client_count, int32_t session_buf_size, int32_t redis_thread_cnt = 1);
	void Terminate();
	bool IsServerRunning()
	{
		return is_server_running_;
	}

private:
	void SetDelegate();
	void OnConnect(int32_t session_idx);
	void OnRecv(int32_t session_idx, const char* p_data, DWORD len);
	void OnDisconnect(int32_t session_idx);

	Network network_;
	bool is_server_running_ = false;

	PacketManager	packet_manager_;
	RoomManager		p_ref_room_manager_;
	UserManager		p_ref_user_manager_;
	RedisManager	p_ref_redis_manager_;
};