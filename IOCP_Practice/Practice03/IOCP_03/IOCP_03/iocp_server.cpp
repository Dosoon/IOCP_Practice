#include "iocp_server.h"

#include <iostream>

bool IOCPServer::InitSocket()
{
	// Winsock 초기화
	WSADATA wsa_data;

	auto start_ret = WSAStartup(MAKEWORD(2, 2), &wsa_data);
	if (start_ret != 0)
	{
		std::cout << "[WSAStartUp] Failed with Error Code : " << start_ret << '\n';
		return false;
	}

	// 소켓 생성
	listen_socket_ = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, NULL, WSA_FLAG_OVERLAPPED);

	if (listen_socket_ == INVALID_SOCKET)
	{
		std::cout << "[ListenSocket] Failed with Error Code : " << WSAGetLastError() << '\n';
		return false;
	}

	std::cout << "[InitSocket] OK\n";
	return true;
}

bool IOCPServer::BindAndListen(const uint16_t port, int32_t backlog_queue_size)
{
	// 소켓 주소 구조체 설정
	SOCKADDR_IN sockaddr;
	sockaddr.sin_family = AF_INET;
	sockaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	sockaddr.sin_port = htons(port);

	// 바인드
	auto bind_ret = bind(listen_socket_, reinterpret_cast<SOCKADDR*>(&sockaddr), sizeof(sockaddr));
	if (bind_ret == SOCKET_ERROR)
	{
		std::cout << "[Bind] Failed with Error Code : " << WSAGetLastError() << '\n';
		return false;
	}

	std::cout << "[Bind] OK\n";

	// 백로그 큐 사이즈 결정
	backlog_queue_size = backlog_queue_size > SOMAXCONN ?
						 SOMAXCONN_HINT(backlog_queue_size) : backlog_queue_size;

	auto listen_ret = listen(listen_socket_, backlog_queue_size);
	if (listen_ret == SOCKET_ERROR)
	{
		std::cout << "[Listen] Failed with Error Code : " << WSAGetLastError() << '\n';
		return false;
	}

	std::cout << "[Listen] OK\n";

	return true;
}

bool IOCPServer::StartServer(const uint32_t max_client_count)
{
	is_server_running_ = true;

	// ClientInfo 풀 생성
	CreateClient(max_client_count);

	// IOCP 생성
	iocp_ = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
	if (iocp_ == NULL)
	{
		std::cout << "[CreateIoCompletionPort] Failed with Error Code : " << GetLastError() << '\n';
		return false;
	}

	// Worker 스레드 생성
	auto create_worker_ret = CreateWorkerThread();
	if (!create_worker_ret)
	{
		std::cout << "[CreateWorkerThread] Failed with Error Code : " << GetLastError() << '\n';
		return false;
	}

	// Accepter 스레드 생성
	auto create_accepter_ret = CreateAccepterThread();
	if (!create_accepter_ret)
	{
		std::cout << "[CreateAccepterThread] Failed with Error Code : " << GetLastError() << '\n';
		return false;
	}

	return true;
}

void IOCPServer::DestroyThread()
{
	// 워커 쓰레드 종료
	is_worker_running_ = false;

	for (auto& worker_thread : worker_thread_list_)
	{
		if (worker_thread.joinable()) {
			worker_thread.join();
		}
	}

	std::cout << "[DestroyThread] Worker Thread Destroyed\n";

	// Accepter 쓰레드 종료
	is_accepter_running_ = false;
	closesocket(listen_socket_);

	if (accepter_thread_.joinable()) {
		accepter_thread_.join();
	}

	std::cout << "[DestroyThread] Accepter Thread Destroyed\n";

	is_server_running_ = false;
	CloseHandle(iocp_);
}

void IOCPServer::CreateClient(const uint32_t max_client_count)
{
	// reallocation 방지를 위한 reserve
	client_info_list_.reserve(max_client_count);

	// ClientInfo 생성
	for (uint32_t i = 0; i < max_client_count; i++)
	{
		client_info_list_.emplace_back();
	}

	std::cout << "[CreateClient] OK\n";
}

bool IOCPServer::CreateWorkerThread()
{
	auto max_worker_thread = GetMaxWorkerThread();

	// reallocation 방지를 위한 reserve
	worker_thread_list_.reserve(max_worker_thread);

	// WorkerThread들 생성
	for (uint32_t i = 0; i < max_worker_thread; i++)
	{
		worker_thread_list_.emplace_back([this]() { WorkerThread(); });
	}

	std::cout << "[CreateWorkerThread] Created " << max_worker_thread << " Threads\n";
	return true;
}

int32_t IOCPServer::GetMaxWorkerThread()
{
	return std::thread::hardware_concurrency() * 2 + 1;
}

bool IOCPServer::CreateAccepterThread()
{
	accepter_thread_ = std::thread([this]() { AccepterThread(); });

	std::cout << "[CreateAccepterThread] OK\n";
	return true;
}

ClientInfo* IOCPServer::GetEmptyClientInfo()
{
	for (auto& client_info : client_info_list_)
	{
		if (client_info.client_socket_ == INVALID_SOCKET) {
			return &client_info;
		}
	}

	return nullptr;
}

bool IOCPServer::BindIOCompletionPort(ClientInfo* p_client_info)
{
	// 클라이언트 소켓을 Completion Port에 바인드
	auto handle = CreateIoCompletionPort(reinterpret_cast<HANDLE>(p_client_info->client_socket_),
									     iocp_, reinterpret_cast<ULONG_PTR>(p_client_info), 0);

	// 바인드 성공여부 확인
	if (!CheckIOCPBindResult(handle)) {
		std::cout << "[BindIOCompletionPort] Failed With Error Code : " << GetLastError() << '\n';
		return false;
	}

	return true;
}

bool IOCPServer::BindRecv(ClientInfo* p_client_info)
{
	DWORD flag = 0;
	DWORD recved_bytes = 0;

	// 버퍼 정보 및 I/O Operation 타입 설정
	p_client_info->recv_overlapped_ex_.wsa_buf_.len = max_sock_buf;
	p_client_info->recv_overlapped_ex_.wsa_buf_.buf = p_client_info->recv_buf_;
	p_client_info->recv_overlapped_ex_.op_type_ = IOOperation::kRECV;

	// Recv 요청
	auto recv_ret = WSARecv(p_client_info->client_socket_,
							&p_client_info->recv_overlapped_ex_.wsa_buf_,
							1, &recved_bytes, &flag,
							&p_client_info->recv_overlapped_ex_.wsa_overlapped_,
							NULL);

	// 에러 처리
	if (recv_ret == SOCKET_ERROR) {
		auto err = WSAGetLastError();

		if (err != ERROR_IO_PENDING) {
			std::cout << "[BindRecv] Failed With Error Code : " << err << '\n';
			return false;
		}
	}

	return true;
}

bool IOCPServer::SendMsg(ClientInfo* p_client_info, char* p_msg, uint32_t len)
{
	DWORD sent_bytes = 0;

	// p_msg -> overlappped 버퍼로 전송할 메시지 복사
	CopyMemory(p_client_info->send_buf_, p_msg, len);

	// 버퍼 정보 및 I/O Operation 타입 설정
	p_client_info->send_overlapped_ex_.wsa_buf_.len = len;
	p_client_info->send_overlapped_ex_.wsa_buf_.buf = p_client_info->send_buf_;
	p_client_info->send_overlapped_ex_.op_type_ = IOOperation::kSEND;

	// Send 요청
	auto send_ret = WSASend(p_client_info->client_socket_,
							&p_client_info->send_overlapped_ex_.wsa_buf_,
							1, &sent_bytes, 0,
							&p_client_info->send_overlapped_ex_.wsa_overlapped_,
							NULL);

	// 에러 처리
	if (send_ret == SOCKET_ERROR) {
		auto err = WSAGetLastError();

		if (err != ERROR_IO_PENDING) {
			std::cout << "[SendMsg] Failed With Error Code : " << err << '\n';
			return false;
		}
	}

	return true;
}

void IOCPServer::WorkerThread()
{
	// CompletionKey
	ClientInfo* p_client_info = NULL;

	// GQCS 반환값
	bool gqcs_ret = false;

	// I/O 처리된 데이터 사이즈
	DWORD io_size = 0;

	// I/O 작업을 위해 요청한 Overlapped 구조체를 받을 포인터
	LPOVERLAPPED p_overlapped = NULL;

	while (is_worker_running_)
	{
		gqcs_ret = GetQueuedCompletionStatus(iocp_, &io_size,
											 (PULONG_PTR)&p_client_info,
											 &p_overlapped, INFINITE);

		// 쓰레드 종료 메시지인지 체크
 		if (TerminateMsg(io_size, p_overlapped)) {
			std::cout << "[WorkerThread] Received Finish Message\n";

			// 다른 쓰레드들도 종료될 수 있도록 메시지 전달
			PostTerminateMsg();
			return;
		}

		// GQCS 결과가 정상 완료 통지인지 체크
		if (!CheckGQCSResult(p_client_info, gqcs_ret, io_size, p_overlapped)) {
			continue;
		}

		// 정상 완료 통지에 대한 처리
		DispatchOverlapped(p_client_info, io_size, p_overlapped);
	}
}

bool IOCPServer::CheckGQCSResult(ClientInfo* p_client_info, bool gqcs_ret, DWORD io_size, LPOVERLAPPED p_overlapped)
{
	// 클라이언트 접속 종료
	if (ClientExited(gqcs_ret, io_size, p_overlapped)) {

		// 접속 종료 전에 수행되어야 할 로직
		OnDisconnect(p_client_info);

		// 소켓 닫고 ClientInfo 초기화
		CloseSocket(p_client_info);

		std::cout << "[WorkerThread] Client Exited\n";
		return false;
	}

	// IOCP 에러
	if (p_overlapped == NULL) {
		std::cout << "[WorkerThread] Null Overlapped\n";
		return false;
	}

	return true;
}


void IOCPServer::DispatchOverlapped(ClientInfo* p_client_info, DWORD io_size, LPOVERLAPPED p_overlapped)
{
	// 확장 Overlapped 구조체로 캐스팅
	OverlappedEx* p_overlapped_ex = reinterpret_cast<OverlappedEx*>(p_overlapped);

	// Recv 완료 통지
	if (p_overlapped_ex->op_type_ == IOOperation::kRECV) {

		// Recv 완료 후 다시 Recv 요청을 건다.
		std::cout << "[WorkerThread] Recv Completion\n";
		OnRecv(p_client_info, io_size);

		if (!BindRecv(p_client_info)) {

			// Recv 요청 실패 시 연결 종료
			std::cout << "[WorkerThread] Failed to Re-bind Recv\n";
			CloseSocket(p_client_info);
		}

		return;
	}

	// Send 완료 통지
	if (p_overlapped_ex->op_type_ == IOOperation::kSEND) {

		// Send 완료
		std::cout << "[WorkerThread] Send Completion : " << io_size << " Bytes\n";

		return;
	}
}

void IOCPServer::AccepterThread()
{
	while (is_accepter_running_)
	{
		// accept될 소켓 데이터를 담을 구조체 세팅
		SOCKADDR_IN accepted_addr;
		int32_t accepted_addr_len = sizeof(accepted_addr);

		// Max Client Count에 달하지 않았다면, ClientInfo 풀에서 하나를 가져옴
		ClientInfo* p_accepted_client_info = GetEmptyClientInfo();
		if (p_accepted_client_info == NULL) {
			std::cout << "[AccepterThread] Current Client Count is MAX\n";
			return;
		}

		// Accept
		SOCKET accepted_socket = accept(listen_socket_,
										reinterpret_cast<SOCKADDR*>(&accepted_addr),
										&accepted_addr_len);

		// 에러 처리
		if (accepted_socket == INVALID_SOCKET) {
			auto err = WSAGetLastError();

			// 종료 메시지가 발생하여 listen_socket이 close된 경우
			if (err == WSAEINTR) {
				std::cout << "[AccepterThread] Accept Interrupted\n";
				return;
			}

			std::cout << "[AccepterThread] Failed With Error Code : " << WSAGetLastError() << '\n';
			continue;
		}

		p_accepted_client_info->client_socket_ = accepted_socket;
		std::cout << "[AccepterThread] New Client Accepted\n";

		// 완료 통지 포트에 바인드
		if (!BindIOCompletionPort(p_accepted_client_info))
		{
			return;
		}

		// Recv 요청
		if (!BindRecv(p_accepted_client_info))
		{
			return;
		}

		// Accept 직후 로직 수행
		OnAccept(p_accepted_client_info);

		++client_cnt_;
	}
}

void IOCPServer::CloseSocket(ClientInfo* p_client_info, bool is_force)
{
	if (p_client_info == NULL) {
		return;
	}

	LINGER linger = { 0, 0 };

	// is_force가 true라면 RST 처리
	if (is_force) {
		linger.l_onoff = 1;
	}

	// Linger 옵션 설정
	setsockopt(p_client_info->client_socket_, SOL_SOCKET,
			   SO_LINGER, reinterpret_cast<const char*>(&linger), sizeof(linger));

	// 소켓 닫고, ClientInfo 초기화 후 풀에 반납
	closesocket(p_client_info->client_socket_);
	ClearClientInfo(p_client_info);

	--client_cnt_;
}

void IOCPServer::ClearClientInfo(ClientInfo* p_client_info)
{
	// ClientInfo 구조체 초기화
	ZeroMemory(&p_client_info->recv_overlapped_ex_, sizeof(OverlappedEx));
	ZeroMemory(&p_client_info->send_overlapped_ex_, sizeof(OverlappedEx));
	p_client_info->client_socket_ = INVALID_SOCKET;
}