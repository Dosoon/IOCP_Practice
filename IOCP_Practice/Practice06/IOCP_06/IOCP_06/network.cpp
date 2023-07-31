#include "network.h"

#include <iostream>

/// <summary>
/// WSAStartup 및 리슨소켓 초기화
/// </summary>
bool Network::InitSocket()
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

/// <summary> <para>
/// 서버 주소 정보를 리슨 소켓에 Bind하고 Listen 처리 </para> <para>
/// 백로그 큐 사이즈를 지정하지 않으면 5로 설정된다. </para>
/// </summary>
bool Network::BindAndListen(const uint16_t port, int32_t backlog_queue_size)
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

/// <summary>
/// Session 풀 생성, IOCP 생성, 워커쓰레드 생성, Accepter 쓰레드 생성
/// </summary>
bool Network::StartNetwork(const uint32_t max_session_cnt, const int32_t session_buf_size)
{
	// Session 풀 생성
	CreateSessionPool(max_session_cnt, session_buf_size);

	// IOCP 생성
	iocp_ = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
	if (iocp_ == NULL) {
		std::cout << "[CreateIoCompletionPort] Failed with Error Code : " << GetLastError() << '\n';
		return false;
	}

	// 워커 쓰레드 및 Accepter 쓰레드 생성
	auto create_ret = CreateThread();
	if (!create_ret) {
		return false;
	}

	return true;
}

/// <summary>
/// 서버 실행에 필요한 워커쓰레드와 Accepter 쓰레드를 생성합니다.
/// </summary>
bool Network::CreateThread()
{
	// Worker 스레드 생성
	auto create_worker_ret = CreateWorkerThread();
	if (!create_worker_ret)
	{
		std::cout << "[CreateWorkerThread] Failed\n";
		return false;
	}

	// Accepter 스레드 생성
	auto create_accepter_ret = CreateAccepterThread();
	if (!create_accepter_ret)
	{
		std::cout << "[CreateAccepterThread] Failed\n";
		return false;
	}

	// Sender 스레드 생성
	auto create_sender_ret = CreateSenderThread();
	if (!create_sender_ret)
	{
		std::cout << "[CreateSenderThread] Failed\n";
		return false;
	}

	return true;
}

/// <summary>
/// 모든 쓰레드를 종료시키고 서버를 종료합니다.
/// </summary>
void Network::Terminate()
{
	// 쓰레드 종료를 유도하는 메시지를 Post
	PostTerminateMsg();

	// 모든 쓰레드의 종료 확인
	DestroyThread();

	// IOCP 핸들 close 및 서버 종료
	CloseHandle(iocp_);
}

/// <summary>
/// 생성된 WorkerThread와 AccepterThread를 모두 파괴한다.
/// </summary>
void Network::DestroyThread()
{
	if (DestroyWorkerThread()) {
		std::cout << "[DestroyThread] Worker Thread Destroyed\n";
	}

	if (DestroyAccepterThread()) {
		std::cout << "[DestroyThread] Accepter Thread Destroyed\n";
	}

	if (DestroySenderThread()) {
		std::cout << "[DestroyThread] Sender Thread Destroyed\n";
	}
}

/// <summary>
/// 워커 쓰레드를 모두 종료시킨다.
/// </summary>
bool Network::DestroyWorkerThread()
{
	// 워커 쓰레드 종료
	is_worker_running_ = false;

	for (auto& worker_thread : worker_thread_list_)
	{
		if (worker_thread.joinable()) {
			worker_thread.join();
		}
	}

	return true;
}

/// <summary>
/// Accepter 쓰레드를 종료시킨다.
/// </summary>
bool Network::DestroyAccepterThread()
{
	// Accepter 쓰레드 종료
	is_accepter_running_ = false;
	closesocket(listen_socket_);

	if (accepter_thread_.joinable()) {
		accepter_thread_.join();
	}

	return true;
}

/// <summary>
/// Sender 쓰레드를 종료시킨다.
/// </summary>
bool Network::DestroySenderThread()
{
	// Sender 쓰레드 종료
	is_sender_running_ = false;
	
	if (sender_thread_.joinable()) {
		sender_thread_.join();
	}

	return true;
}

/// <summary>
/// Session 풀에 Session들을 생성한다.
/// </summary>
void Network::CreateSessionPool(const int32_t max_session_cnt, const int32_t session_buf_size)
{
	// reallocation 방지를 위한 reserve
	session_list_.reserve(max_session_cnt);

	// Session 생성
	for (int32_t i = 0; i < max_session_cnt; i++)
	{
		session_list_.emplace_back(new Session(i, session_buf_size));
	}

	std::cout << "[CreateSessionPool] OK\n";
}

/// <summary>
/// WorkerThread 리스트에 WorkerThread들을 생성한다.
/// </summary>
bool Network::CreateWorkerThread()
{
	auto max_worker_thread = GetMaxWorkerThread();

	// reallocation 방지를 위한 reserve
	worker_thread_list_.reserve(max_worker_thread);

	// WorkerThread들 생성
	for (int32_t i = 0; i < max_worker_thread; i++)
	{
		worker_thread_list_.emplace_back([this]() { WorkerThread(); });
	}

	std::cout << "[CreateWorkerThread] Created " << max_worker_thread << " Threads\n";
	return true;
}

/// <summary>
/// AccepterThread를 생성한다.
/// </summary>
bool Network::CreateAccepterThread()
{
	accepter_thread_ = std::thread([this]() { AccepterThread(); });

	std::cout << "[CreateAccepterThread] OK\n";
	return true;
}

/// <summary>
/// SenderThread를 생성한다.
/// </summary>
bool Network::CreateSenderThread()
{
	sender_thread_ = std::thread([this]() { SenderThread(); });

	std::cout << "[CreateSenderThread] OK\n";
	return true;
}

/// <summary>
/// Session 풀에서 미사용 Session 구조체를 반환한다.
/// </summary>
Session* Network::GetEmptySession()
{
	for (auto& session : session_list_)
	{
		// CAS 연산을 통해 세션 활성화 여부를 확인하고, 사용되고 있음을 Flag 변수로 남김
		bool activated_expected = false;
		if (session->is_activated_
					 .compare_exchange_strong(activated_expected, true)) {
			return session;
		}
	}

	return nullptr;
}

/// <summary>
/// IOCP 객체에 세션 소켓 및 CompletionKey를 연결한다.
/// </summary>
bool Network::BindIOCompletionPort(Session* p_session)
{
	// 세션 소켓을 Completion Port에 바인드
	auto handle = CreateIoCompletionPort(reinterpret_cast<HANDLE>(p_session->socket_),
									     iocp_, reinterpret_cast<ULONG_PTR>(p_session), 0);

	// 바인드 성공여부 확인
	if (!CheckIOCPBindResult(handle)) {
		std::cout << "[BindIOCompletionPort] Failed With Error Code : " << GetLastError() << '\n';
		return false;
	}

	return true;
}

/// <summary>
/// WSARecv Overlapped I/O 작업을 요청한다.
/// </summary>
bool Network::BindRecv(Session* p_session)
{
	DWORD flag = 0;
	DWORD recved_bytes = 0;

	// 버퍼 정보 및 I/O Operation 타입 설정
	p_session->recv_overlapped_ex_.wsa_buf_.len = p_session->buf_size_;
	p_session->recv_overlapped_ex_.wsa_buf_.buf = p_session->recv_buf_;
	p_session->recv_overlapped_ex_.op_type_ = IOOperation::kRECV;

	// Recv 요청
	auto recv_ret = WSARecv(p_session->socket_,
							&p_session->recv_overlapped_ex_.wsa_buf_,
							1, &recved_bytes, &flag,
							&p_session->recv_overlapped_ex_.wsa_overlapped_,
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

/// <summary>
/// Overlapped I/O 작업 완료 통지를 처리하는 워커 쓰레드
/// </summary>
void Network::WorkerThread()
{
	// CompletionKey
	Session* p_session = NULL;

	// GQCS 반환값
	bool gqcs_ret = false;

	// I/O 처리된 데이터 사이즈
	DWORD io_size = 0;

	// I/O 작업을 위해 요청한 Overlapped 구조체를 받을 포인터
	LPOVERLAPPED p_overlapped = NULL;

	while (is_worker_running_)
	{
		gqcs_ret = GetQueuedCompletionStatus(iocp_, &io_size,
											 (PULONG_PTR)&p_session,
											 &p_overlapped, INFINITE);

		// 쓰레드 종료 메시지인지 체크
 		if (TerminateMsg(gqcs_ret, io_size, p_overlapped)) {
			std::cout << "[WorkerThread] Received Finish Message\n";

			// 다른 쓰레드들도 종료될 수 있도록 메시지 전달
			PostTerminateMsg();
			return;
		}

		// GQCS 결과가 정상 완료 통지인지 체크
		if (!CheckGQCSResult(p_session, gqcs_ret, io_size, p_overlapped)) {
			continue;
		}

		// 정상 완료 통지에 대한 처리
		DispatchOverlapped(p_session, io_size, p_overlapped);
	}
}

/// <summary>
/// GetQueuedCompletionStatus 결과에 따라 정상 처리 여부를 반환한다.
/// </summary>
bool Network::CheckGQCSResult(Session* p_session, bool gqcs_ret, DWORD io_size, LPOVERLAPPED p_overlapped)
{
	// 세션 접속 종료
	if (SessionExited(gqcs_ret, io_size, p_overlapped)) {

		// 접속 종료 전에 수행되어야 할 로직
		OnDisconnect(p_session);

		// 소켓 닫고 Session 초기화
		CloseSocket(p_session);

		std::cout << "[WorkerThread] Session Exited\n";
		return false;
	}

	// IOCP 에러
	if (p_overlapped == NULL) {
		std::cout << "[WorkerThread] Null Overlapped\n";
		return false;
	}

	return true;
}

/// <summary>
/// Overlapped의 I/O 타입에 따라 Recv 혹은 Send 완료 루틴을 수행한다.
/// </summary>
void Network::DispatchOverlapped(Session* p_session, DWORD io_size, LPOVERLAPPED p_overlapped)
{
	// 확장 Overlapped 구조체로 캐스팅
	OverlappedEx* p_overlapped_ex = reinterpret_cast<OverlappedEx*>(p_overlapped);

	// Recv 완료 통지
	if (p_overlapped_ex->op_type_ == IOOperation::kRECV) {

		// Recv 완료 후 다시 Recv 요청을 건다.
		std::cout << "[WorkerThread] Recv Completion\n";
		OnRecv(p_session, io_size);

		if (!BindRecv(p_session)) {

			// Recv 요청 실패 시 연결 종료
			std::cout << "[WorkerThread] Failed to Re-bind Recv\n";
			CloseSocket(p_session);
		}

		return;
	}

	// Send 완료 통지
	if (p_overlapped_ex->op_type_ == IOOperation::kSEND) {

		// Send 완료
		std::cout << "[WorkerThread] Send Completion : " << io_size << " Bytes\n";

		// OverlappedEx 삭제
		delete p_overlapped_ex;

		// Send 사용 여부 해제
		p_session->is_sending_.store(0);

		return;
	}
}

/// <summary>
/// 동기 Accept 작업을 처리하는 Accepter 쓰레드
/// </summary>
void Network::AccepterThread()
{
	while (is_accepter_running_)
	{
		// accept될 소켓 데이터를 담을 구조체 세팅
		SOCKADDR_IN accepted_addr;
		int32_t accepted_addr_len = sizeof(accepted_addr);

		// Max Session Count에 달하지 않았다면, Session 풀에서 하나를 가져옴
		Session* p_accepted_session = GetEmptySession();
		if (p_accepted_session == NULL) {
			std::cout << "[AccepterThread] Current Session Count is MAX\n";
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

		p_accepted_session->socket_ = accepted_socket;
		std::cout << "[AccepterThread] New Session Accepted\n";

		// 완료 통지 포트에 바인드
		if (!BindIOCompletionPort(p_accepted_session))
		{
			return;
		}

		// Recv 요청
		if (!BindRecv(p_accepted_session))
		{
			return;
		}

		// Accept 직후 로직 수행
		OnAccept(p_accepted_session);

		++session_cnt_;
	}
}

/// <summary>
/// 세션의 Send 작업을 수행하는 Sender 쓰레드
/// </summary>
void Network::SenderThread()
{
	while (is_sender_running_)
	{
		for (auto& session : session_list_)
		{
			// 보낼 데이터가 없다면 패스
			if (!session->HasSendData()) {
				continue;
			}
			// Send 요청, 실패시 연결 종료
			else {
				if (!session->BindSend()) {
					CloseSocket(session);
				}
			}
		}
	}
}

/// <summary> <para>
/// 소켓 연결을 종료시킨다. </para><para>
/// is_force가 true라면 RST 처리한다. </para>
/// </summary>
void Network::CloseSocket(Session* p_session, bool is_force)
{
	// NULL 체크
	if (p_session == NULL) {
		return;
	}

	// CloseSocket 중복 호출 방지
	bool activated_expected = true;
	if (p_session->is_activated_
				   .compare_exchange_strong(activated_expected, false) == false) {
		return;
	}

	// 기본 Linger 옵션 세팅
	LINGER linger = { 0, 0 };

	// is_force가 true라면 RST 처리
	if (is_force) {
		linger.l_onoff = 1;
	}

	// Linger 옵션 설정
	setsockopt(p_session->socket_, SOL_SOCKET,
			   SO_LINGER, reinterpret_cast<const char*>(&linger), sizeof(linger));

	// 소켓 닫고, Session 초기화 후 풀에 반납
	closesocket(p_session->socket_);
	ClearSession(p_session);

	--session_cnt_;
}

/// <summary>
/// Session 구조체를 재사용하기 위해 초기화한다.
/// </summary>
void Network::ClearSession(Session* p_session)
{
	// 소켓 초기화
	p_session->socket_ = INVALID_SOCKET;

	// Session 구조체 초기화
	ZeroMemory(&p_session->recv_overlapped_ex_, sizeof(OverlappedEx));

	// Send 링 버퍼 초기화
	p_session->send_buf_.ClearBuffer();

	// Send 사용 여부 초기화
	p_session->is_sending_ = false;
}