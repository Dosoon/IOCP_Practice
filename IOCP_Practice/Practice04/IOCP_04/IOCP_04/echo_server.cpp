#include "echo_server.h"

#include <iostream>

/// <summary> <para>
/// �����ص� �ݹ��Լ��� ��Ʈ��ũ Ŭ������ �����ϰ�, </para> <para>
/// ���� ������ ���� �� ��Ʈ��ũ�� �����Ѵ�. </para>
/// </summary>
bool EchoServer::Start(int32_t max_session_cnt, int32_t session_buf_size)
{
	is_server_running_ = true;

	network_.SetOnAccept(std::bind(&EchoServer::OnAccept, this, std::placeholders::_1));
	network_.SetOnRecv(std::bind(&EchoServer::OnRecv, this, std::placeholders::_1, std::placeholders::_2));
	network_.SetOnDisconnect(std::bind(&EchoServer::OnDisconnect, this, std::placeholders::_1));

	if (!CreatePacketProcessThread()) {
		std::cout << "[StartServer] Failed to Create Packet Process Thread\n";
		return false;
	}

	if (!network_.InitSocket()) {
		std::cout << "[StartServer] Failed to Initialize\n";
		return false;
	}

	if (!network_.BindAndListen(7777)) {
		std::cout << "[StartServer] Failed to Bind And Listen\n";
		return false;
	}

	if (!network_.StartNetwork(max_session_cnt, session_buf_size)) {
		std::cout << "[StartServer] Failed to Start\n";
		return false;
	}

	std::cout << "[StartServer] Successed to Start Server\n";
	return true;
}

/// <summary>
/// ��Ʈ��ũ Ŭ������ �����Ű�� ������ �����Ѵ�.
/// </summary>
void EchoServer::Terminate()
{
	network_.Terminate();
	is_server_running_ = false;
}

/// <summary>
/// ��Ʈ��ũ �ܿ��� Accept�ÿ� ����Ǵ� �ݹ� �Լ��̴�.
/// </summary>
void EchoServer::OnAccept(Session* p_session)
{
	char ip[16];
	uint16_t port;

	// IP �� ��Ʈ��ȣ ��������
	if (GetSessionIpPort(p_session, ip, sizeof(ip), port))
	{
		// ���ο� ���� �α� ���
		std::cout << "[OnAccept] " << ip << ":" << port << " Entered" << "\n";
	}
}

/// <summary>
/// ��Ʈ��ũ �ܿ��� Recv�ÿ� ����Ǵ� �ݹ� �Լ��̴�.
/// </summary>
void EchoServer::OnRecv(Session* p_session, DWORD io_size)
{
	// ���������� ���� �޽��� ���
	p_session->recv_buf_[io_size] = '\0';
	std::cout << "[OnRecvMsg] " << p_session->recv_buf_ << "\n";

	// ���� �޽����� �״�� �������Ѵ�.
	network_.SendMsg(p_session, p_session->recv_buf_, io_size);
}

/// <summary>
/// ��Ʈ��ũ �ܿ��� ���� Disconnect�ÿ� ����Ǵ� �ݹ� �Լ��̴�.
/// </summary>
void EchoServer::OnDisconnect(Session* p_session)
{
	char ip[16];
	uint16_t port;

	// IP �� ��Ʈ��ȣ ��������
	if (GetSessionIpPort(p_session, ip, sizeof(ip), port))
	{
		// ���� ���� �α� ���
		std::cout << "[OnDisconnect] " << ip << ":" << port << " Leaved" << "\n";
	}
}

/// <summary>
/// ���� �����͸� ���� IP �ּҿ� ��Ʈ ��ȣ�� �����´�.
/// </summary>
bool EchoServer::GetSessionIpPort(Session* p_session, char* ip_dest, int32_t ip_len, uint16_t& port_dest)
{
	// Peer ���� ��������
	SOCKADDR_IN session_addr;
	int32_t session_addr_len = sizeof(session_addr);
	
	int32_t peerResult = getpeername(p_session->socket_, (sockaddr*)&session_addr, &session_addr_len);
	if (peerResult == SOCKET_ERROR)
	{
		std::cout << "[GetClientIpPort] Failed to Get Peer Name\n";
		return false;
	}

	// IP �ּ� ���ڿ��� ��ȯ
	inet_ntop(AF_INET, &session_addr.sin_addr, ip_dest, ip_len);

	// ��Ʈ ����
	port_dest = session_addr.sin_port;

	return true;
}

/// <summary>
/// Packet ó�� �����带 �����Ѵ�.
/// </summary>
bool EchoServer::CreatePacketProcessThread()
{
	packet_process_thread_ = std::thread([this]() { PacketProcessThread(); });

	std::cout << "[CreatePacketProcessThread] OK\n";
	return true;
}

/// <summary>
/// packet_deque_���� ��Ŷ�� �ϳ��� deque�� ó���ϴ�
/// Packet Process ������ ������ �����Ѵ�.
/// </summary>
void EchoServer::PacketProcessThread()
{
	while (is_server_running_)
	{
		// ��Ŷ ��ť
		Packet packet = DequePacket();

		// ��Ŷ ó�� ����
		if (packet.data_size == 0) {
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
		else {
			network_.SendMsg(packet.session_index_, packet.data, packet.data_size);
		}
	}
}