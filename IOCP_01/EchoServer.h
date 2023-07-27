#pragma once
#include "i_o_completion_port.h"

class EchoServer : public IOCompletionPort
{
public:
	virtual void OnRecvMsg(ClientInfo* p_client_info, DWORD io_size) override
	{
		// 받은 메시지를 그대로 재전송한다.
		SendMsg(p_client_info, p_client_info->recv_overlapped_ex_.buf_, io_size);
	}
	virtual void OnSendMsg(ClientInfo* p_client_info, DWORD io_size) override
	{
		
	}
	virtual void OnAccept(ClientInfo* p_client_info) override
	{

	}
};