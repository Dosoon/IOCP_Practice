#include "chat_server.h"

#include <iostream>
#include <mutex>

/// <summary> <para>
/// 정의해둔 콜백함수를 네트워크 클래스에 세팅하고, </para> <para>
/// 로직 스레드 생성 후 네트워크를 시작한다. </para>
/// </summary>
bool ChatServer::Start(uint16_t port, int32_t max_session_cnt, int32_t session_buf_size)
{
	is_server_running_ = true;

	// 네트워크에서 Accept, Recv, Disconnect 발생 시 에코 서버에서 수행할 콜백함수 로직 세팅
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
/// 네트워크 클래스와 패킷 처리 쓰레드를 종료시킨 후, 서버를 종료한다.
/// </summary>
void ChatServer::Terminate()
{
	network_.Terminate();

	DestroyPacketProcessThread();
}

/// <summary>
/// 네트워크 단에서 Accept시에 수행되는 콜백 함수이다.
/// </summary>
void ChatServer::OnConnect(int32_t session_idx)
{
	// 세션 데이터 불러오기
	auto session = network_.GetSessionByIdx(session_idx);

	// 세션 어드레스 불러오기
	char ip[16];
	uint16_t port;
	session->GetSessionIpPort(ip, 16, port);

	// 새로운 접속 로그 출력
	std::cout << "[OnConnect] ID " << session_idx << " Entered (" << ip << ":" << port << ")\n";
	
}

/// <summary>
/// 네트워크 단에서 Recv시에 수행되는 콜백 함수이다.
/// </summary>
void ChatServer::OnRecv(int32_t session_idx, const char* p_data, DWORD len)
{
	std::cout << "[OnRecvMsg] " << std::string_view(p_data, len) << "\n";

	// 패킷 처리 스레드로 전달
	EnqueuePacket(session_idx, len, const_cast<char*>(p_data));
}

/// <summary>
/// 네트워크 단에서 세션 Disconnect시에 수행되는 콜백 함수이다.
/// </summary>
void ChatServer::OnDisconnect(int32_t session_idx)
{
	// 세션 데이터 불러오기
	auto session = network_.GetSessionByIdx(session_idx);

	// 세션 어드레스 불러오기
	char ip[16];
	uint16_t port;
	session->GetSessionIpPort(ip, 16, port);

	// 새로운 접속 로그 출력
	std::cout << "[OnDisconnect] ID " << session_idx << " Leaved (" << ip << ":" << port << ")\n";
}

/// <summary>
/// Packet 처리 쓰레드를 생성한다.
/// </summary>
bool ChatServer::CreatePacketProcessThread()
{
	packet_process_thread_ = std::thread([this]() { PacketProcessThread(); });

	return true;
}

/// <summary>
/// Packet 처리 쓰레드를 종료한다.
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
/// packet_deque_에서 패킷을 하나씩 deque해 처리하는
/// Packet Process 쓰레드 내용을 정의한다.
/// </summary>
void ChatServer::PacketProcessThread()
{
	while (is_server_running_)
	{
		// 패킷 디큐
		auto packet = DequePacket();

		// 패킷 처리 로직
		// 패킷이 없다면 1ms간 Block
		if (packet.data_size == 0) {
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
		// 에코 서버 로직 : 세션 Send 링 버퍼에 데이터 Enqueue
		else {
			auto session = network_.GetSessionByIdx(packet.session_index_);
			session->EnqueueSendData(packet.data, packet.data_size);

			// 패킷 데이터 삭제
			delete[] packet.data;
		}
	}
}

/// <summary>
/// Packet Deque에 새 패킷을 추가한다.
/// </summary>
void ChatServer::EnqueuePacket(int32_t session_index, int32_t len, char* data_src)
{
	// 패킷 덱에 락 걸기
	std::lock_guard<std::mutex> lock(packet_deque_lock_);
	
	// 패킷 데이터(에코 메시지)를 저장할 새로운 메모리 할당
	auto packet_data = new char[len];
	CopyMemory(packet_data, data_src, len);

	// 패킷 덱에 패킷 추가
	packet_deque_.emplace_back(session_index, len, packet_data);
}

/// <summary>
/// Packet Deque에서 가장 먼저 들어온 패킷을 하나 Deque하고, 없으면 빈 패킷을 반환한다.
/// </summary>
Packet ChatServer::DequePacket()
{
	// 패킷 덱에 락 걸기
	std::lock_guard<std::mutex> lock(packet_deque_lock_);

	// 덱이 비어있다면, 빈 패킷을 리턴해 Sleep 유도
	if (packet_deque_.empty()) {
		return Packet();
	}
	// 패킷이 있다면, 맨 앞의 것을 리턴
	else {
		auto packet = packet_deque_.front();
		packet_deque_.pop_front();
		return packet;
	}
}
