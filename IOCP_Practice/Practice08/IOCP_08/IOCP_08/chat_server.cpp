#include "chat_server.h"

#include <iostream>
#include <mutex>

/// <summary> <para>
/// �����ص� �ݹ��Լ��� ��Ʈ��ũ Ŭ������ �����ϰ�, </para> <para>
/// ���� ������ ���� �� ��Ʈ��ũ�� �����Ѵ�. </para>
/// </summary>
bool ChatServer::Start(uint16_t port, int32_t max_session_cnt, int32_t session_buf_size)
{
	is_server_running_ = true;

	// ��Ʈ��ũ���� Accept, Recv, Disconnect �߻� �� ���� �������� ������ �ݹ��Լ� ���� ����
	network_.SetOnConnect(std::bind(&ChatServer::OnConnect, this, std::placeholders::_1));
	network_.SetOnRecv(std::bind(&ChatServer::OnRecv, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	network_.SetOnDisconnect(std::bind(&ChatServer::OnDisconnect, this, std::placeholders::_1));

	if (!CreatePacketProcessThread()) {
		std::cout << "[StartServer] Failed to Create Packet Process Thread\n";
		return false;
	}

	if (!network_.Start(port, max_session_cnt, session_buf_size)) {
		std::cout << "[StartServer] Failed to Start\n";
		return false;
	}

	std::cout << "[StartServer] Server Started\n";
	return true;
}

/// <summary>
/// ��Ʈ��ũ Ŭ������ ��Ŷ ó�� �����带 �����Ų ��, ������ �����Ѵ�.
/// </summary>
void ChatServer::Terminate()
{
	network_.Terminate();

	DestroyPacketProcessThread();
}

/// <summary>
/// ��Ʈ��ũ �ܿ��� Accept�ÿ� ����Ǵ� �ݹ� �Լ��̴�.
/// </summary>
void ChatServer::OnConnect(int32_t session_idx)
{
	// ���� ������ �ҷ�����
	auto session = network_.GetSessionByIdx(session_idx);

	// ���� ��巹�� �ҷ�����
	char ip[16];
	uint16_t port;
	session->GetSessionIpPort(ip, 16, port);

	// ���ο� ���� �α� ���
	std::cout << "[OnConnect] ID " << session_idx << " Entered (" << ip << ":" << port << ")\n";
	
}

/// <summary>
/// ��Ʈ��ũ �ܿ��� Recv�ÿ� ����Ǵ� �ݹ� �Լ��̴�.
/// </summary>
void ChatServer::OnRecv(int32_t session_idx, const char* p_data, DWORD len)
{
	std::cout << "[OnRecvMsg] " << std::string_view(p_data, len) << "\n";

	// ��Ŷ ó�� ������� ����
	EnqueuePacket(session_idx, len, const_cast<char*>(p_data));
}

/// <summary>
/// ��Ʈ��ũ �ܿ��� ���� Disconnect�ÿ� ����Ǵ� �ݹ� �Լ��̴�.
/// </summary>
void ChatServer::OnDisconnect(int32_t session_idx)
{
	// ���� ������ �ҷ�����
	auto session = network_.GetSessionByIdx(session_idx);

	// ���� ��巹�� �ҷ�����
	char ip[16];
	uint16_t port;
	session->GetSessionIpPort(ip, 16, port);

	// ���ο� ���� �α� ���
	std::cout << "[OnDisconnect] ID " << session_idx << " Leaved (" << ip << ":" << port << ")\n";
}

/// <summary>
/// Packet ó�� �����带 �����Ѵ�.
/// </summary>
bool ChatServer::CreatePacketProcessThread()
{
	packet_process_thread_ = std::thread([this]() { PacketProcessThread(); });

	return true;
}

/// <summary>
/// Packet ó�� �����带 �����Ѵ�.
/// </summary>
bool ChatServer::DestroyPacketProcessThread()
{
	is_server_running_ = false;

	if (packet_process_thread_.joinable())
	{
		packet_process_thread_.join();
	}

	std::cout << "[DestroyPacketProcessThread] Packet Process Thread Destroyed\n";
	return true;
}

/// <summary>
/// packet_deque_���� ��Ŷ�� �ϳ��� deque�� ó���ϴ�
/// Packet Process ������ ������ �����Ѵ�.
/// </summary>
void ChatServer::PacketProcessThread()
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
void ChatServer::EnqueuePacket(int32_t session_index, int32_t len, char* data_src)
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
Packet ChatServer::DequePacket()
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
