#include "network.h"

#include <iostream>

/// <summary>
/// WSAStartup 및 리슨소켓 초기화
/// </summary>
bool Network::InitListenSocket()
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
	if (!ErrorHandler(bind_ret, WSAGetLastError(), "Bind", 0)) {
		return false;
	}

	// 백로그 큐 사이즈 결정
	backlog_queue_size = backlog_queue_size > SOMAXCONN ?
						 SOMAXCONN_HINT(backlog_queue_size) : backlog_queue_size;

	auto listen_ret = listen(listen_socket_, backlog_queue_size);
	if (!ErrorHandler(listen_ret, WSAGetLastError(), "Listen", 0)) {
		return false;
	}

	// IOCP에 바인드
	auto iocp_ret = CreateIoCompletionPort(reinterpret_cast<HANDLE>(listen_socket_), iocp_, 0, 0);
	if (!ErrorHandler(iocp_ret, GetLastError(), "CreateIoCompletionPort", iocp_)) {
		return false;
	}

	return true;
}

/// <summary>
/// 서버를 시작한다.
/// </summary>
bool Network::Start(uint16_t port, const uint32_t max_session_cnt, const int32_t session_buf_size)
{
	if (!InitListenSocket()) {
		std::cout << "[Start] Failed to Initialize Socket\n";
		return false;
	}

	// Session 풀 생성
	CreateSessionPool(max_session_cnt, session_buf_size);

	// IOCP 생성
	if (!CreateIOCP()) {
		std::cout << "[Start] Failed to Create IOCP\n";
		return false;
	}

	// 워커 쓰레드 및 Accepter 쓰레드 생성
	auto create_ret = CreateWorkerAndIOThread();
	if (!create_ret) {
		std::cout << "[Start] Failed to Create Threads\n";
		return false;
	}

	// Bind & Listen
	if (!BindAndListen(port)) {
		std::cout << "[Start] Failed to Bind And Listen\n";
		return false;
	}

	return true;
}

/// <summary>
/// Session 풀 생성, IOCP 생성, 워커쓰레드 생성, Accepter 쓰레드 생성
/// </summary>
bool Network::CreateIOCP()
{
	// IOCP 생성
	iocp_ = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
	if (iocp_ == NULL) {
		std::cout << "[CreateIoCompletionPort] Failed with Error Code : " << GetLastError() << '\n';
		return false;
	}

	return true;
}

/// <summary>
/// 서버 실행에 필요한 워커쓰레드와 Accepter 쓰레드를 생성합니다.
/// </summary>
bool Network::CreateWorkerAndIOThread()
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

	// 세션 풀 할당 해제
	for (auto& session : session_list_) {
		delete session;
	}

	// IOCP 핸들 close 및 서버 종료
	CloseHandle(iocp_);
}

/// <summary>
/// 생성된 WorkerThread와 AccepterThread를 모두 파괴한다.
/// </summary>
void Network::DestroyThread()
{
	DestroyWorkerThread();
	DestroyAccepterThread();
	DestroySenderThread();
}

/// <summary>
/// 워커 쓰레드를 모두 종료시킨다.
/// </summary>
void Network::DestroyWorkerThread()
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
}

/// <summary>
/// Accepter 쓰레드를 종료시킨다.
/// </summary>
void Network::DestroyAccepterThread()
{
	// Accepter 쓰레드 종료
	is_accepter_running_ = false;
	closesocket(listen_socket_);

	if (accepter_thread_.joinable()) {
		accepter_thread_.join();
	}

	std::cout << "[DestroyThread] Accepter Thread Destroyed\n";
}

/// <summary>
/// Sender 쓰레드를 종료시킨다.
/// </summary>
void Network::DestroySenderThread()
{
	// Sender 쓰레드 종료
	is_sender_running_ = false;
	
	if (sender_thread_.joinable()) {
		sender_thread_.join();
	}

	std::cout << "[DestroyThread] Sender Thread Destroyed\n";
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

	return true;
}

/// <summary>
/// AccepterThread를 생성한다.
/// </summary>
bool Network::CreateAccepterThread()
{
	accepter_thread_ = std::thread([this]() { AccepterThread(); });

	return true;
}

/// <summary>
/// SenderThread를 생성한다.
/// </summary>
bool Network::CreateSenderThread()
{
	sender_thread_ = std::thread([this]() { SenderThread(); });

	return true;
}

/// <summary>
/// IOCP 객체에 세션 소켓 및 CompletionKey를 연결한다.
/// </summary>
bool Network::BindIOCompletionPort(Session* p_session)
{
	// 세션 소켓을 Completion Port에 바인드
	auto handle = CreateIoCompletionPort(reinterpret_cast<HANDLE>(p_session->GetSocket()),
									     iocp_, reinterpret_cast<ULONG_PTR>(p_session), 0);

	// 바인드 성공여부 확인
	if (!ErrorHandler(handle, GetLastError(), "CreateIoCompletionPort", iocp_)) {
		return false;
	}

	return true;
}

/// <summary>
/// Overlapped I/O 작업 완료 통지를 처리하는 워커 쓰레드
/// </summary>
void Network::WorkerThread()
{
	while (is_worker_running_)
	{
		Session* p_session = NULL;
		bool gqcs_ret = false;
		DWORD io_size = 0;
		LPOVERLAPPED p_overlapped = NULL;

		gqcs_ret = GetQueuedCompletionStatus(iocp_, &io_size,
			reinterpret_cast<PULONG_PTR>(&p_session), &p_overlapped, INFINITE);

		// 쓰레드 종료 메시지인지 체크
 		if (TerminateMsg(gqcs_ret, io_size, p_overlapped)) {
			std::cout << "[WorkerThread] Received Finish Message\n";

			// 다른 쓰레드들도 종료될 수 있도록 메시지 전달 후, 이 스레드 종료
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
		OnDisconnect(p_session->index_);

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
/// Overlapped의 I/O 타입에 따라 완료 루틴을 수행한다.
/// </summary>
void Network::DispatchOverlapped(Session* p_session, DWORD io_size, LPOVERLAPPED p_overlapped)
{
	// 확장 Overlapped 구조체로 캐스팅
	OverlappedEx* p_overlapped_ex = reinterpret_cast<OverlappedEx*>(p_overlapped);

	// Recv 완료 통지
	if (p_overlapped_ex->op_type_ == IOOperation::kRECV) {

		// Recv 완료 후 다시 Recv 요청을 건다.
		OnRecv(p_session->index_, p_session->recv_buf_, io_size);

		if (!p_session->BindRecv()) {

			// Recv 요청 실패 시 연결 종료
			std::cout << "[WorkerThread] Failed to Re-bind Recv\n";
			CloseSocket(p_session);
		}

		return;
	}

	// Send 완료 통지
	if (p_overlapped_ex->op_type_ == IOOperation::kSEND) {

		// Send 사용 여부 해제
		p_session->is_sending_.store(0);

		return;
	}

	// Accept 완료 통지
	if (p_overlapped_ex->op_type_ == IOOperation::kACCEPT) {
		p_session = GetSessionByIdx(p_overlapped_ex->session_idx_);

		// IOCP에 바인드
		if (BindIOCompletionPort(p_session)) {
			OnConnect(p_session->index_);

			p_session->Activate();

			// Recv 바인드
			if (!p_session->BindRecv()) {
				CloseSocket(p_session);
			}

			++session_cnt_;
		}
		else {
			CloseSocket(p_session);
		}
	}
}

/// <summary>
/// 비동기 Accept 작업을 처리하는 Accepter 쓰레드
/// </summary>
void Network::AccepterThread()
{
	while (is_accepter_running_)
	{
		auto cur_time = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now().time_since_epoch()).count();

		for (auto& session : session_list_)
		{
			// 이미 사용 중인 세션
			if (session->is_activated_.load()) {
				continue;
			}

			// 사용 가능한 시간인지 확인
			if (static_cast<unsigned long long>(cur_time) < session->latest_conn_closed_) {
				continue;
			}

			auto diff = cur_time - session->latest_conn_closed_;
			if (diff <= SESSION_REUSE_TIME) {
				continue;
			}

			// Accept 비동기 요청
			session->BindAccept(listen_socket_);
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(64));
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
	if (p_session == NULL) {
		return;
	}

	// 재사용을 잠시 방지하기 위해 MAX값 설정
	p_session->SetConnectionClosedTime(UINT64_MAX);

	// CloseSocket 중복 호출 방지
	bool activated_expected = true;
	if (p_session->is_activated_
		.compare_exchange_strong(activated_expected, false) == false) {
		return;
	}

	// is_force 여부에 따라 RST 처리
	LINGER linger = { is_force, 0 };

	setsockopt(p_session->GetSocket(), SOL_SOCKET, SO_LINGER,
		reinterpret_cast<const char*>(&linger), sizeof(linger));

	// 소켓 닫고, Session 초기화 후 풀에 반납
	closesocket(p_session->GetSocket());
	ClearSession(p_session);

	--session_cnt_;
}

/// <summary>
/// Session 구조체를 재사용하기 위해 초기화한다.
/// </summary>
void Network::ClearSession(Session* p_session)
{
	p_session->SetSocket(INVALID_SOCKET);
	ZeroMemory(&p_session->recv_overlapped_ex_, sizeof(OverlappedEx));
	p_session->ClearSendBuffer();
	p_session->is_sending_.store(false);
	auto cur_time = std::chrono::duration_cast<std::chrono::seconds>
					(std::chrono::steady_clock::now().time_since_epoch()).count();
	p_session->SetConnectionClosedTime(cur_time);
}

/// <summary> <para>
/// 허용되는 에러 코드가 아니라면 로그를 남긴다. </para> <para>
/// 로그를 남겼다면 false를 리턴한다. </para> <para>
/// allow_codes의 첫 번째 인자는 허용되는 에러 코드의 개수이다. </para>
/// </summary>
bool Network::ErrorHandler(bool result, int32_t error_code, const char* method, int32_t allow_codes, ...)
{
	if (result) {
		return true;
	}
	else {
		va_list code_list;
		va_start(code_list, allow_codes);

		for (auto i = 0; i < allow_codes; ++i) {
			if (error_code == va_arg(code_list, int32_t)) {
				return true;
			}
		}

		std::cout << '[' << method << "] Failed With Error Code : " << error_code << '\n';
		va_end(code_list);
		return false;
	}
}

bool Network::ErrorHandler(int32_t socket_result, int32_t error_code, const char* method, int32_t allow_codes, ...)
{
	if (socket_result != SOCKET_ERROR) {
		return true;
	}
	else {
		va_list code_list;
		va_start(code_list, allow_codes);

		for (auto i = 0; i < allow_codes; ++i) {
			if (error_code == va_arg(code_list, int32_t)) {
				return true;
			}
		}

		std::cout << '[' << method << "] Failed With Error Code : " << error_code << '\n';
		va_end(code_list);
		return true;
	}
}

bool Network::ErrorHandler(HANDLE handle_result, int32_t error_code, const char* method, HANDLE allow_handle)
{
	if (handle_result == allow_handle) {
		return true;
	}
	else {
		std::cout << '[' << method << "] Failed With Error Code : " << error_code << '\n';
		return false;
	}
}