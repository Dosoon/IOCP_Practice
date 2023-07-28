#pragma once
#include "i_o_completion_port.h"

class EchoServer : public IOCompletionPort
{
public:
	virtual void OnRecvMsg(ClientInfo* p_client_info, DWORD io_size) override
	{
		// 디버깅용으로 받은 메시지 출력
		p_client_info->recv_buf_[io_size] = '\0';
		std::cout << "[OnRecvMsg] " << p_client_info->recv_buf_ << "\n";

		// 받은 메시지를 그대로 재전송한다.
		SendMsg(p_client_info, p_client_info->recv_buf_, io_size);
	}
	virtual void OnSendMsg(ClientInfo* p_client_info, DWORD io_size) override
	{

	}
	virtual void OnAccept(ClientInfo* p_client_info) override
	{

	}
};