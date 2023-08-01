#include "network.h"

#include <iostream>

/// <summary>
/// WSAStartup �� �������� �ʱ�ȭ
/// </summary>
bool Network::InitSocket()
{
	// Winsock �ʱ�ȭ
	WSADATA wsa_data;

	auto start_ret = WSAStartup(MAKEWORD(2, 2), &wsa_data);
	if (start_ret != 0)
	{
		std::cout << "[WSAStartUp] Failed with Error Code : " << start_ret << '\n';
		return false;
	}

	// ���� ����
	listen_socket_ = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, NULL, WSA_FLAG_OVERLAPPED);

	if (listen_socket_ == INVALID_SOCKET)
	{
		std::cout << "[ListenSocket] Failed with Error Code : " << WSAGetLastError() << '\n';
		return false;
	}

	return true;
}

/// <summary> <para>
/// ���� �ּ� ������ ���� ���Ͽ� Bind�ϰ� Listen ó�� </para> <para>
/// ��α� ť ����� �������� ������ 5�� �����ȴ�. </para>
/// </summary>
bool Network::BindAndListen(const uint16_t port, int32_t backlog_queue_size)
{
	// ���� �ּ� ����ü ����
	SOCKADDR_IN sockaddr;
	sockaddr.sin_family = AF_INET;
	sockaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	sockaddr.sin_port = htons(port);

	// ���ε�
	auto bind_ret = bind(listen_socket_, reinterpret_cast<SOCKADDR*>(&sockaddr), sizeof(sockaddr));
	if (!ErrorHandler(bind_ret, WSAGetLastError(), "Bind", 0)) {
		return false;
	}

	// ��α� ť ������ ����
	backlog_queue_size = backlog_queue_size > SOMAXCONN ?
						 SOMAXCONN_HINT(backlog_queue_size) : backlog_queue_size;

	auto listen_ret = listen(listen_socket_, backlog_queue_size);
	if (!ErrorHandler(listen_ret, WSAGetLastError(), "Listen", 0)) {
		return false;
	}

	// IOCP�� ���ε�
	auto iocp_ret = CreateIoCompletionPort(reinterpret_cast<HANDLE>(listen_socket_), iocp_, 0, 0);
	if (!ErrorHandler(iocp_ret, GetLastError(), "CreateIoCompletionPort", iocp_)) {
		return false;
	}

	return true;
}

/// <summary>
/// ������ �����Ѵ�.
/// </summary>
bool Network::Start(uint16_t port, const uint32_t max_session_cnt, const int32_t session_buf_size)
{
	if (!InitSocket()) {
		std::cout << "[Start] Failed to Initialize Socket\n";
		return false;
	}

	// Session Ǯ ����
	CreateSessionPool(max_session_cnt, session_buf_size);

	// IOCP ����
	if (!CreateIOCP()) {
		std::cout << "[Start] Failed to Create IOCP\n";
		return false;
	}

	// ��Ŀ ������ �� Accepter ������ ����
	auto create_ret = CreateThread();
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
/// Session Ǯ ����, IOCP ����, ��Ŀ������ ����, Accepter ������ ����
/// </summary>
bool Network::CreateIOCP()
{
	// IOCP ����
	iocp_ = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
	if (iocp_ == NULL) {
		std::cout << "[CreateIoCompletionPort] Failed with Error Code : " << GetLastError() << '\n';
		return false;
	}

	return true;
}

/// <summary>
/// ���� ���࿡ �ʿ��� ��Ŀ������� Accepter �����带 �����մϴ�.
/// </summary>
bool Network::CreateThread()
{
	// Worker ������ ����
	auto create_worker_ret = CreateWorkerThread();
	if (!create_worker_ret)
	{
		std::cout << "[CreateWorkerThread] Failed\n";
		return false;
	}

	// Accepter ������ ����
	auto create_accepter_ret = CreateAccepterThread();
	if (!create_accepter_ret)
	{
		std::cout << "[CreateAccepterThread] Failed\n";
		return false;
	}

	// Sender ������ ����
	auto create_sender_ret = CreateSenderThread();
	if (!create_sender_ret)
	{
		std::cout << "[CreateSenderThread] Failed\n";
		return false;
	}

	return true;
}

/// <summary>
/// ��� �����带 �����Ű�� ������ �����մϴ�.
/// </summary>
void Network::Terminate()
{
	// ������ ���Ḧ �����ϴ� �޽����� Post
	PostTerminateMsg();

	// ��� �������� ���� Ȯ��
	DestroyThread();

	// IOCP �ڵ� close �� ���� ����
	CloseHandle(iocp_);
}

/// <summary>
/// ������ WorkerThread�� AccepterThread�� ��� �ı��Ѵ�.
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
/// ��Ŀ �����带 ��� �����Ų��.
/// </summary>
bool Network::DestroyWorkerThread()
{
	// ��Ŀ ������ ����
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
/// Accepter �����带 �����Ų��.
/// </summary>
bool Network::DestroyAccepterThread()
{
	// Accepter ������ ����
	is_accepter_running_ = false;
	closesocket(listen_socket_);

	if (accepter_thread_.joinable()) {
		accepter_thread_.join();
	}

	return true;
}

/// <summary>
/// Sender �����带 �����Ų��.
/// </summary>
bool Network::DestroySenderThread()
{
	// Sender ������ ����
	is_sender_running_ = false;
	
	if (sender_thread_.joinable()) {
		sender_thread_.join();
	}

	return true;
}

/// <summary>
/// Session Ǯ�� Session���� �����Ѵ�.
/// </summary>
void Network::CreateSessionPool(const int32_t max_session_cnt, const int32_t session_buf_size)
{
	// reallocation ������ ���� reserve
	session_list_.reserve(max_session_cnt);

	// Session ����
	for (int32_t i = 0; i < max_session_cnt; i++)
	{
		session_list_.emplace_back(new Session(i, session_buf_size));
	}
}

/// <summary>
/// WorkerThread ����Ʈ�� WorkerThread���� �����Ѵ�.
/// </summary>
bool Network::CreateWorkerThread()
{
	auto max_worker_thread = GetMaxWorkerThread();

	// reallocation ������ ���� reserve
	worker_thread_list_.reserve(max_worker_thread);

	// WorkerThread�� ����
	for (int32_t i = 0; i < max_worker_thread; i++)
	{
		worker_thread_list_.emplace_back([this]() { WorkerThread(); });
	}

	return true;
}

/// <summary>
/// AccepterThread�� �����Ѵ�.
/// </summary>
bool Network::CreateAccepterThread()
{
	accepter_thread_ = std::thread([this]() { AccepterThread(); });

	return true;
}

/// <summary>
/// SenderThread�� �����Ѵ�.
/// </summary>
bool Network::CreateSenderThread()
{
	sender_thread_ = std::thread([this]() { SenderThread(); });

	return true;
}

/// <summary>
/// IOCP ��ü�� ���� ���� �� CompletionKey�� �����Ѵ�.
/// </summary>
bool Network::BindIOCompletionPort(Session* p_session)
{
	// ���� ������ Completion Port�� ���ε�
	auto handle = CreateIoCompletionPort(reinterpret_cast<HANDLE>(p_session->socket_),
									     iocp_, reinterpret_cast<ULONG_PTR>(p_session), 0);

	// ���ε� �������� Ȯ��
	if (!ErrorHandler(handle, GetLastError(), "CreateIoCompletionPort", iocp_)) {
		return false;
	}

	return true;
}

/// <summary>
/// WSARecv Overlapped I/O �۾��� ��û�Ѵ�.
/// </summary>
bool Network::BindRecv(Session* p_session)
{
	DWORD flag = 0;
	DWORD recved_bytes = 0;

	// ���� ���� �� I/O Operation Ÿ�� ����
	SetRecvOverlappedEx(p_session);

	// Recv ��û
	auto recv_ret = WSARecv(p_session->socket_,
							&p_session->recv_overlapped_ex_.wsa_buf_,
							1, &recved_bytes, &flag,
							&p_session->recv_overlapped_ex_.wsa_overlapped_,
							NULL);

	// ���� ó��
	if (!ErrorHandler(recv_ret, WSAGetLastError(), "WSARecv", 1, ERROR_IO_PENDING)) {
		return false;
	}

	return true;
}

/// <summary>
/// RecvOverlappedEx�� ���� ���� �� I/O Operation Ÿ���� �����Ѵ�.
/// </summary>
void Network::SetRecvOverlappedEx(Session* p_session)
{
	p_session->recv_overlapped_ex_.socket_ = p_session->socket_;
	p_session->recv_overlapped_ex_.wsa_buf_.len = p_session->buf_size_;
	p_session->recv_overlapped_ex_.wsa_buf_.buf = p_session->recv_buf_;
	p_session->recv_overlapped_ex_.op_type_ = IOOperation::kRECV;
}

/// <summary>
/// Overlapped I/O �۾� �Ϸ� ������ ó���ϴ� ��Ŀ ������
/// </summary>
void Network::WorkerThread()
{
	// CompletionKey
	Session* p_session = NULL;

	// GQCS ��ȯ��
	bool gqcs_ret = false;

	// I/O ó���� ������ ������
	DWORD io_size = 0;

	// I/O �۾��� ���� ��û�� Overlapped ����ü�� ���� ������
	LPOVERLAPPED p_overlapped = NULL;

	while (is_worker_running_)
	{
		gqcs_ret = GetQueuedCompletionStatus(iocp_, &io_size,
			reinterpret_cast<PULONG_PTR>(&p_session), &p_overlapped, INFINITE);

		// ������ ���� �޽������� üũ
 		if (TerminateMsg(gqcs_ret, io_size, p_overlapped)) {
			std::cout << "[WorkerThread] Received Finish Message\n";

			// �ٸ� ������鵵 ����� �� �ֵ��� �޽��� ���� ��, �� ������ ����
			PostTerminateMsg();
			return;
		}

		// GQCS ����� ���� �Ϸ� �������� üũ
		if (!CheckGQCSResult(p_session, gqcs_ret, io_size, p_overlapped)) {
			continue;
		}

		// ���� �Ϸ� ������ ���� ó��
		DispatchOverlapped(p_session, io_size, p_overlapped);
	}
}

/// <summary>
/// GetQueuedCompletionStatus ����� ���� ���� ó�� ���θ� ��ȯ�Ѵ�.
/// </summary>
bool Network::CheckGQCSResult(Session* p_session, bool gqcs_ret, DWORD io_size, LPOVERLAPPED p_overlapped)
{
	// ���� ���� ����
	if (SessionExited(gqcs_ret, io_size, p_overlapped)) {

		// ���� ���� ���� ����Ǿ�� �� ����
		OnDisconnect(p_session->index_);

		// ���� �ݰ� Session �ʱ�ȭ
		CloseSocket(p_session);

		std::cout << "[WorkerThread] Session Exited\n";
		return false;
	}

	// IOCP ����
	if (p_overlapped == NULL) {
		std::cout << "[WorkerThread] Null Overlapped\n";
		return false;
	}

	return true;
}

/// <summary>
/// Overlapped�� I/O Ÿ�Կ� ���� �Ϸ� ��ƾ�� �����Ѵ�.
/// </summary>
void Network::DispatchOverlapped(Session* p_session, DWORD io_size, LPOVERLAPPED p_overlapped)
{
	// Ȯ�� Overlapped ����ü�� ĳ����
	OverlappedEx* p_overlapped_ex = reinterpret_cast<OverlappedEx*>(p_overlapped);

	// Recv �Ϸ� ����
	if (p_overlapped_ex->op_type_ == IOOperation::kRECV) {

		// Recv �Ϸ� �� �ٽ� Recv ��û�� �Ǵ�.
		OnRecv(p_session->index_, p_session->recv_buf_, io_size);

		if (!BindRecv(p_session)) {

			// Recv ��û ���� �� ���� ����
			std::cout << "[WorkerThread] Failed to Re-bind Recv\n";
			CloseSocket(p_session);
		}

		return;
	}

	// Send �Ϸ� ����
	if (p_overlapped_ex->op_type_ == IOOperation::kSEND) {

		// Send ��� ���� ����
		p_session->is_sending_.store(0);

		return;
	}

	// Accept �Ϸ� ����
	if (p_overlapped_ex->op_type_ == IOOperation::kACCEPT) {
		p_session = GetSessionByIdx(p_overlapped_ex->session_idx_);

		// IOCP�� ���ε�
		if (BindIOCompletionPort(p_session)) {
			OnConnect(p_session->index_);

			// ���� Ȱ��ȭ
			p_session->is_activated_.store(true);

			++session_cnt_;

			// Recv ���ε�
			if (!BindRecv(p_session)) {
				CloseSocket(p_session);
			}
		}
		else {
			CloseSocket(p_session);
		}
	}
}

/// <summary>
/// �񵿱� Accept �۾��� ó���ϴ� Accepter ������
/// </summary>
void Network::AccepterThread()
{
	while (is_accepter_running_)
	{
		auto cur_time = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now().time_since_epoch()).count();

		for (auto& session : session_list_)
		{
			// �̹� ��� ���� ����
			if (session->is_activated_.load()) {
				continue;
			}

			// ��� ������ �ð����� Ȯ��
			if (static_cast<unsigned long long>(cur_time) < session->latest_conn_closed_) {
				continue;
			}

			auto diff = cur_time - session->latest_conn_closed_;
			if (diff <= SESSION_REUSE_TIME) {
				continue;
			}

			// Accept �񵿱� ��û
			session->BindAccept(listen_socket_);
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(64));
	}
}

/// <summary>
/// ������ Send �۾��� �����ϴ� Sender ������
/// </summary>
void Network::SenderThread()
{
	while (is_sender_running_)
	{
		for (auto& session : session_list_)
		{
			// ���� �����Ͱ� ���ٸ� �н�
			if (!session->HasSendData()) {
				continue;
			}
			// Send ��û, ���н� ���� ����
			else {
				if (!session->BindSend()) {
					CloseSocket(session);
				}
			}
		}
	}
}

/// <summary> <para>
/// ���� ������ �����Ų��. </para><para>
/// is_force�� true��� RST ó���Ѵ�. </para>
/// </summary>
void Network::CloseSocket(Session* p_session, bool is_force)
{
	if (p_session == NULL) {
		return;
	}

	// ������ ��� �����ϱ� ���� MAX�� ����
	p_session->latest_conn_closed_ = UINT64_MAX;

	// CloseSocket �ߺ� ȣ�� ����
	bool activated_expected = true;
	if (p_session->is_activated_
		.compare_exchange_strong(activated_expected, false) == false) {
		return;
	}

	// �⺻ Linger �ɼ� ����
	LINGER linger = { 0, 0 };

	// is_force�� true��� RST ó��
	if (is_force) {
		linger.l_onoff = 1;
	}

	// Linger �ɼ� ����
	setsockopt(p_session->socket_, SOL_SOCKET, SO_LINGER,
		reinterpret_cast<const char*>(&linger), sizeof(linger));

	// ���� �ݰ�, Session �ʱ�ȭ �� Ǯ�� �ݳ�
	closesocket(p_session->socket_);
	ClearSession(p_session);

	--session_cnt_;
}

/// <summary>
/// Session ����ü�� �����ϱ� ���� �ʱ�ȭ�Ѵ�.
/// </summary>
void Network::ClearSession(Session* p_session)
{
	// ���� �ʱ�ȭ
	p_session->socket_ = INVALID_SOCKET;

	// OverlappedEx �ʱ�ȭ
	ZeroMemory(&p_session->recv_overlapped_ex_, sizeof(OverlappedEx));
	ZeroMemory(&p_session->send_overlapped_ex_, sizeof(OverlappedEx));
	ZeroMemory(&p_session->accept_overlapped_ex_, sizeof(OverlappedEx));

	// Send �� ���� �� �ɰ� �ʱ�ȭ
	std::lock_guard<std::mutex> lock(p_session->send_lock_);
	p_session->send_buf_.ClearBuffer();

	// Send ��� ���� �ʱ�ȭ
	p_session->is_sending_ = false;

	// ������ ���� �ð� ����
	p_session->latest_conn_closed_ = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

/// <summary> <para>
/// ���Ǵ� ���� �ڵ尡 �ƴ϶�� �α׸� �����. </para> <para>
/// �α׸� ����ٸ� false�� �����Ѵ�. </para> <para>
/// allow_codes�� ù ��° ���ڴ� ���Ǵ� ���� �ڵ��� �����̴�. </para>
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