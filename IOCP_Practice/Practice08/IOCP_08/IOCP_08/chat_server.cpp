#include "chat_server.h"

#include <iostream>
#include <mutex>

#include "packet_id.h"

/// <summary> <para>
/// �����ص� �ݹ��Լ��� ��Ʈ��ũ Ŭ������ �����ϰ�, </para> <para>
/// ���� ������ ���� �� ��Ʈ��ũ�� �����Ѵ�. </para>
/// </summary>
bool ChatServer::Start(uint16_t port, int32_t max_session_cnt, int32_t session_buf_size, int32_t redis_thread_cnt)
{
	is_server_running_ = true;

	SetDelegate();

	if (!network_.Start(port, max_session_cnt, session_buf_size)) {
		std::cout << "[StartServer] Failed to Start\n";
		return false;
	}

	packet_manager_.Init(max_session_cnt, redis_thread_cnt);
	packet_manager_.Run();

	std::cout << "[StartServer] Server Started\n";
	return true;
}

/// <summary>
/// ��Ʈ��ũ Ŭ������ ��Ŷ �Ŵ����� �����Ų ��, ������ �����Ѵ�.
/// </summary>
void ChatServer::Terminate()
{
	network_.Terminate();
	packet_manager_.Terminate();
	is_server_running_ = false;
}

/// <summary>
/// ��Ʈ��ũ �ܿ��� ���� Delegate �Լ��� ����
/// </summary>
void ChatServer::SetDelegate()
{
	// ��Ʈ��ũ���� Accept, Recv, Disconnect �߻� �� ���� �������� ������ Delegate ����
	network_.SetOnConnect(std::bind(&ChatServer::OnConnect, this, std::placeholders::_1));
	network_.SetOnRecv(std::bind(&ChatServer::OnRecv, this,
					   std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	network_.SetOnDisconnect(std::bind(&ChatServer::OnDisconnect, this, std::placeholders::_1));

	// ��Ŷ �Ŵ������� ���� SendPacket �Լ� ����
	packet_manager_.SetSendPacket(std::bind(&Network::SendPacket, &network_,
								  std::placeholders::_1, std::placeholders::_2,
								  std::placeholders::_3));
}

/// <summary>
/// ��Ʈ��ũ �ܿ��� Accept�ÿ� �����
/// </summary>
void ChatServer::OnConnect(int32_t session_idx)
{
	// ���� ������ �ҷ�����
	auto session = network_.GetSessionByIdx(session_idx);

	// ���� ��巹�� �ҷ�����
	char ip[16];
	uint16_t port;
	session->GetSessionIpPort(ip, 16, port);

	// ��Ŷ �Ŵ����� �ý��� ��Ŷ ����
	PacketInfo pkt { 
		.session_index_ = static_cast<uint16_t>(session_idx),
		.id_ = static_cast<int32_t>(PACKET_ID::kSYS_USER_CONNECT), 
		.data_size_ = 0,
		.data_ = nullptr
	};

	packet_manager_.EnqueueSystemPacket(pkt);

	// ���ο� ���� �α� ���
	std::cout << "[OnConnect] ID " << session_idx << " Entered (" << ip << ":" << port << ")\n";
}

/// <summary>
/// ��Ʈ��ũ �ܿ��� Recv�ÿ� �����
/// </summary>
void ChatServer::OnRecv(int32_t session_idx, const char* p_data, DWORD len)
{
	// TODO : ����
	std::cout << "[OnRecvMsg] " << std::string_view(p_data, len) << "\n";

	// ��Ŷ �Ŵ����� ����
	packet_manager_.EnqueuePacket(session_idx, p_data, len);
}

/// <summary>
/// ��Ʈ��ũ �ܿ��� ���� Disconnect�ÿ� �����
/// </summary>
void ChatServer::OnDisconnect(int32_t session_idx)
{
	// ���� ������ �ҷ�����
	auto session = network_.GetSessionByIdx(session_idx);

	// ���� ��巹�� �ҷ�����
	char ip[16];
	uint16_t port;
	session->GetSessionIpPort(ip, 16, port);

	// ��Ŷ �Ŵ����� �ý��� ��Ŷ ����
	PacketInfo pkt{
		.session_index_ = static_cast<uint16_t>(session_idx),
		.id_ = static_cast<int32_t>(PACKET_ID::kSYS_USER_DISCONNECT),
		.data_size_ = 0,
		.data_ = nullptr
	};

	packet_manager_.EnqueueSystemPacket(pkt);

	// ���ο� ���� �α� ���
	std::cout << "[OnDisconnect] ID " << session_idx << " Leaved (" << ip << ":" << port << ")\n";
}