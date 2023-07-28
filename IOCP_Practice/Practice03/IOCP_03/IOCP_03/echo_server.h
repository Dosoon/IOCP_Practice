#pragma once
#include "iocp_server.h"

class EchoServer : public IOCPServer
{
private:
	// IOCompletionPort을(를) 통해 상속됨
	virtual void OnAccept(ClientInfo* p_client_info) override;
	virtual void OnRecv(ClientInfo* p_client_info, DWORD io_size) override;
	virtual void OnDisconnect(ClientInfo* p_client_info) override;

	bool GetClientIpPort(ClientInfo* p_client_info, char* ip_dest, int32_t ip_len, uint16_t& port_dest);
};