#pragma once
#include "network.h"

class EchoServer
{
public:
	bool Start();
	void Terminate();
	bool IsServerRunning()
	{
		return is_server_running_;
	}

private:
	Network network_;

	void OnAccept(Session* p_session);
	void OnRecv(Session* p_session, DWORD io_size);
	void OnDisconnect(Session* p_session);
	bool GetClientIpPort(Session* p_session, char* ip_dest, int32_t ip_len, uint16_t& port_dest);

	bool is_server_running_ = false;
};