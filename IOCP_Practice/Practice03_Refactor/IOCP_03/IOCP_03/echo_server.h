#pragma once
#include "network.h"

class EchoServer
{
public:
	bool Start(int32_t max_client_count, int32_t session_buf_size);
	void Terminate();
	bool IsServerRunning()
	{
		return is_server_running_;
	}

private:
	void OnAccept(Session* p_session);
	void OnRecv(Session* p_session, DWORD io_size);
	void OnDisconnect(Session* p_session);
	bool GetSessionIpPort(Session* p_session, char* ip_dest, int32_t ip_len, uint16_t& port_dest);

	Network network_;
	bool is_server_running_ = false;
};