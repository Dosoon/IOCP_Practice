#pragma once
#include <deque>
#include <mutex>

#include "packet.h"
#include "network.h"

class EchoServer
{
public:
	bool Start(uint16_t port, int32_t max_client_count, int32_t session_buf_size);
	void Terminate();
	bool IsServerRunning()
	{
		return is_server_running_;
	}

private:
	void OnConnect(int32_t session_idx);
	void OnRecv(int32_t session_idx, const char* p_data, DWORD len);
	void OnDisconnect(int32_t session_idx);
	bool CreatePacketProcessThread();
	bool DestroyPacketProcessThread();
	void PacketProcessThread();
	void EnqueuePacket(int32_t session_index, int32_t len, char* data_src);
	Packet DequePacket();

	Network network_;
	bool is_server_running_ = false;

	std::thread packet_process_thread_;
	std::deque<Packet> packet_deque_;
	std::mutex packet_deque_lock_;
};