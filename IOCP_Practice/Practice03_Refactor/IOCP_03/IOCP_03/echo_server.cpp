#include "echo_server.h"

#include <iostream>

bool EchoServer::Start()
{
	is_server_running_ = true;

	network_.SetOnAccept(std::bind(&EchoServer::OnAccept, this, std::placeholders::_1));
	network_.SetOnRecv(std::bind(&EchoServer::OnRecv, this, std::placeholders::_1, std::placeholders::_2));
	network_.SetOnDisconnect(std::bind(&EchoServer::OnDisconnect, this, std::placeholders::_1));

	if (!network_.InitSocket())
	{
		std::cout << "[StartServer] Failed to Initialize\n";
		return -1;
	}

	if (!network_.BindAndListen(7777))
	{
		std::cout << "[StartServer] Failed to Bind And Listen\n";
		return -1;
	}

	if (!network_.StartNetwork(1000))
	{
		std::cout << "[StartServer] Failed to Start\n";
		return -1;
	}

	std::cout << "[StartServer] Successed to Start Server\n";
}

void EchoServer::Terminate()
{
	network_.Terminate();
	is_server_running_ = false;
}

void EchoServer::OnAccept(Session* p_client_info)
{
	char ip[16];
	uint16_t port;

	// IP �� ��Ʈ��ȣ ��������
	if (GetClientIpPort(p_client_info, ip, sizeof(ip), port))
	{
		// ���ο� ���� �α� ���
		std::cout << "[OnAccept] " << ip << ":" << port << " Entered" << "\n";
	}
}

void EchoServer::OnRecv(Session* p_client_info, DWORD io_size)
{
	// ���������� ���� �޽��� ���
	p_client_info->recv_buf_[io_size] = '\0';
	std::cout << "[OnRecvMsg] " << p_client_info->recv_buf_ << "\n";

	// ���� �޽����� �״�� �������Ѵ�.
	network_.SendMsg(p_client_info, p_client_info->recv_buf_, io_size);
}

void EchoServer::OnDisconnect(Session* p_client_info)
{
	char ip[16];
	uint16_t port;

	// IP �� ��Ʈ��ȣ ��������
	if (GetClientIpPort(p_client_info, ip, sizeof(ip), port))
	{
		// ���� ���� �α� ���
		std::cout << "[OnDisconnect] " << ip << ":" << port << " Leaved" << "\n";
	}
}

bool EchoServer::GetClientIpPort(Session* p_client_info, char* ip_dest, int32_t ip_len, uint16_t& port_dest)
{
	// Client Peer ���� ��������
	SOCKADDR_IN client_addr;
	int32_t client_addr_len = sizeof(client_addr);
	
	int32_t peerResult = getpeername(p_client_info->client_socket_, (sockaddr*)&client_addr, &client_addr_len);
	if (peerResult == SOCKET_ERROR)
	{
		std::cout << "[GetClientIpPort] Failed to Get Peer Name\n";
		return false;
	}

	// IP �ּ� ���ڿ��� ��ȯ
	inet_ntop(AF_INET, &client_addr.sin_addr, ip_dest, ip_len);

	// ��Ʈ ����
	port_dest = client_addr.sin_port;

	return true;
}
