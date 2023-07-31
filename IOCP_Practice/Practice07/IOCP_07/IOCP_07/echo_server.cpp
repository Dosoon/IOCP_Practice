#include "echo_server.h"

#include <iostream>
#include <mutex>

/// <summary> <para>
/// 정의해둔 콜백함수를 네트워크 클래스에 세팅하고, </para> <para>
/// 로직 스레드 생성 후 네트워크를 시작한다. </para>
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
/// 네트워크 클래스와 패킷 처리 쓰레드를 종료시킨 후, 서버를 종료한다.
/// </summary>
void EchoServer::Terminate()
{
	network_.Terminate();

	DestroyPacketProcessThread();
}

/// <summary>
/// 네트워크 단에서 Accept시에 수행되는 콜백 함수이다.
/// </summary>
void EchoServer::OnAccept(Session* p_session)
{
	char ip[16];
	uint16_t port;

	// IP 및 포트번호 가져오기
	if (GetSessionIpPort(p_session, ip, sizeof(ip), port))
	{
		// 새로운 접속 로그 출력
		std::cout << "[OnAccept] " << ip << ":" << port << " Entered" << "\n";
	}
}

/// <summary>
/// 네트워크 단에서 Recv시에 수행되는 콜백 함수이다.
/// </summary>
void EchoServer::OnRecv(Session* p_session, DWORD io_size)
{
	// 디버깅용으로 받은 메시지 출력
	p_session->recv_buf_[io_size] = '\0';
	std::cout << "[OnRecvMsg] " << p_session->recv_buf_ << "\n";

	// 패킷 처리 스레드로 전달
	EnqueuePacket(p_session->index_, io_size, p_session->recv_buf_);
}

/// <summary>
/// 네트워크 단에서 세션 Disconnect시에 수행되는 콜백 함수이다.
/// </summary>
void EchoServer::OnDisconnect(Session* p_session)
{
	char ip[16];
	uint16_t port;

	// IP 및 포트번호 가져오기
	if (GetSessionIpPort(p_session, ip, sizeof(ip), port))
	{
		// 접속 종료 로그 출력
		std::cout << "[OnDisconnect] " << ip << ":" << port << " Leaved" << "\n";
	}
}

/// <summary>
/// 세션 데이터를 토대로 IP 주소와 포트 번호를 가져온다.
/// </summary>
bool EchoServer::GetSessionIpPort(Session* p_session, char* ip_dest, int32_t ip_len, uint16_t& port_dest)
{
	// Peer 정보 가져오기
	SOCKADDR_IN* local_addr = NULL, *session_addr = NULL;
	int32_t session_addr_len = sizeof(session_addr);
	
	GetAcceptExSockaddrs(p_session->accept_buf_, 0, sizeof(SOCKADDR_IN) + 16, sizeof(SOCKADDR_IN) + 16,
							reinterpret_cast<SOCKADDR**>(&local_addr), &session_addr_len,
							reinterpret_cast<SOCKADDR**>(&session_addr), &session_addr_len);

	// IP 주소 문자열로 변환
	inet_ntop(AF_INET, &session_addr->sin_addr, ip_dest, ip_len);

	// 포트 정보
	port_dest = session_addr->sin_port;

	return true;
}

/// <summary>
/// Packet 처리 쓰레드를 생성한다.
/// </summary>
bool EchoServer::CreatePacketProcessThread()
{
	packet_process_thread_ = std::thread([this]() { PacketProcessThread(); });

	std::cout << "[CreatePacketProcessThread] OK\n";
	return true;
}

/// <summary>
/// Packet 처리 쓰레드를 종료한다.
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
/// packet_deque_에서 패킷을 하나씩 deque해 처리하는
/// Packet Process 쓰레드 내용을 정의한다.
/// </summary>
void EchoServer::PacketProcessThread()
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
void EchoServer::EnqueuePacket(int32_t session_index, int32_t len, char* data_src)
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
Packet EchoServer::DequePacket()
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
