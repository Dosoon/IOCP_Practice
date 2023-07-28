#include "echo_server.h"

#include <iostream>

void EchoServer::OnAccept(ClientInfo* p_client_info)
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

void EchoServer::OnRecv(ClientInfo* p_client_info, DWORD io_size)
{
	// ���������� ���� �޽��� ���
	p_client_info->recv_buf_[io_size] = '\0';
	std::cout << "[OnRecvMsg] " << p_client_info->recv_buf_ << "\n";

	// ���� �޽����� �״�� �������Ѵ�.
	SendMsg(p_client_info, p_client_info->recv_buf_, io_size);
}

void EchoServer::OnDisconnect(ClientInfo* p_client_info)
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

bool EchoServer::GetClientIpPort(ClientInfo* p_client_info, char* ip_dest, int32_t ip_len, uint16_t& port_dest)
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
