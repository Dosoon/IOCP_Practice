#include "echo_server.h"

#include <iostream>
#include <mutex>

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

	if (!network_.StartNetwork(max_session_cnt, session_buf_size)) {
		std::cout << "[StartServer] Failed to Start\n";
		return false;
	}

	if (!network_.BindAndListen(7777)) {
		std::cout << "[StartServer] Failed to Bind And Listen\n";
		return false;
	}

	std::cout << "[StartServer] Successed to Start Server\n";
	return true;
}

/// <summary>
/// ��Ʈ��ũ Ŭ������ ��Ŷ ó�� �����带 �����Ų ��, ������ �����Ѵ�.
/// </summary>
void EchoServer::Terminate()
{
	network_.Terminate();

	DestroyPacketProcessThread();
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

	// ��Ŷ ó�� ������� ����
	EnqueuePacket(p_session->index_, io_size, p_session->recv_buf_);
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
	SOCKADDR_IN* local_addr = NULL, *session_addr = NULL;
	int32_t session_addr_len = sizeof(session_addr);
	
	GetAcceptExSockaddrs(p_session->accept_buf_, 0, sizeof(SOCKADDR_IN) + 16, sizeof(SOCKADDR_IN) + 16,
							reinterpret_cast<SOCKADDR**>(&local_addr), &session_addr_len,
							reinterpret_cast<SOCKADDR**>(&session_addr), &session_addr_len);

	// IP �ּ� ���ڿ��� ��ȯ
	inet_ntop(AF_INET, &session_addr->sin_addr, ip_dest, ip_len);

	// ��Ʈ ����
	port_dest = session_addr->sin_port;

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
/// Packet ó�� �����带 �����Ѵ�.
/// </summary>
bool EchoServer::DestroyPacketProcessThread()
{
	is_server_running_ = false;

	if (packet_process_thread_.joinable())
	{
		packet_process_thread_.join();
	}

	std::cout << "[DestroyPacketProcessThread] OK\n";
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
		auto packet = DequePacket();

		// ��Ŷ ó�� ����
		// ��Ŷ�� ���ٸ� 1ms�� Block
		if (packet.data_size == 0) {
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
		// ���� ���� ���� : ���� Send �� ���ۿ� ������ Enqueue
		else {
			auto session = network_.GetSessionByIdx(packet.session_index_);
			session->EnqueueSendData(packet.data, packet.data_size);

			// ��Ŷ ������ ����
			delete[] packet.data;
		}
	}
}

/// <summary>
/// Packet Deque�� �� ��Ŷ�� �߰��Ѵ�.
/// </summary>
void EchoServer::EnqueuePacket(int32_t session_index, int32_t len, char* data_src)
{
	// ��Ŷ ���� �� �ɱ�
	std::lock_guard<std::mutex> lock(packet_deque_lock_);
	
	// ��Ŷ ������(���� �޽���)�� ������ ���ο� �޸� �Ҵ�
	auto packet_data = new char[len];
	CopyMemory(packet_data, data_src, len);

	// ��Ŷ ���� ��Ŷ �߰�
	packet_deque_.emplace_back(session_index, len, packet_data);
}

/// <summary>
/// Packet Deque���� ���� ���� ���� ��Ŷ�� �ϳ� Deque�ϰ�, ������ �� ��Ŷ�� ��ȯ�Ѵ�.
/// </summary>
Packet EchoServer::DequePacket()
{
	// ��Ŷ ���� �� �ɱ�
	std::lock_guard<std::mutex> lock(packet_deque_lock_);

	// ���� ����ִٸ�, �� ��Ŷ�� ������ Sleep ����
	if (packet_deque_.empty()) {
		return Packet();
	}
	// ��Ŷ�� �ִٸ�, �� ���� ���� ����
	else {
		auto packet = packet_deque_.front();
		packet_deque_.pop_front();
		return packet;
	}
}
