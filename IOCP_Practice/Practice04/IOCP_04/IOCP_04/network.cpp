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

	std::cout << "[InitSocket] OK\n";
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
	if (bind_ret == SOCKET_ERROR)
	{
		std::cout << "[Bind] Failed with Error Code : " << WSAGetLastError() << '\n';
		return false;
	}

	std::cout << "[Bind] OK\n";

	// ��α� ť ������ ����
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
/// Session Ǯ ����, IOCP ����, ��Ŀ������ ����, Accepter ������ ����
/// </summary>
bool Network::StartNetwork(const uint32_t max_session_cnt, const int32_t session_buf_size)
{
	// Session Ǯ ����
	CreateSessionPool(max_session_cnt, session_buf_size);

	// IOCP ����
	iocp_ = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
	if (iocp_ == NULL) {
		std::cout << "[CreateIoCompletionPort] Failed with Error Code : " << GetLastError() << '\n';
		return false;
	}

	// ��Ŀ ������ �� Accepter ������ ����
	auto create_ret = CreateThread();
	if (!create_ret) {
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
}

/// <summary>
/// ��Ŀ �����带 ��� �����ŵ�ϴ�.
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
/// Accepter �����带 �����ŵ�ϴ�.
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
/// Session Ǯ�� Session���� �����Ѵ�.
/// </summary>
void Network::CreateSessionPool(const int32_t max_session_cnt, const int32_t session_buf_size)
{
	// reallocation ������ ���� reserve
	session_list_.reserve(max_session_cnt);

	// Session ����
	for (int32_t i = 0; i < max_session_cnt; i++)
	{
		session_list_.emplace_back(i, session_buf_size);
	}

	std::cout << "[CreateSessionPool] OK\n";
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

	std::cout << "[CreateWorkerThread] Created " << max_worker_thread << " Threads\n";
	return true;
}

/// <summary>
/// AccepterThread�� �����Ѵ�.
/// </summary>
bool Network::CreateAccepterThread()
{
	accepter_thread_ = std::thread([this]() { AccepterThread(); });

	std::cout << "[CreateAccepterThread] OK\n";
	return true;
}

/// <summary>
/// Session Ǯ���� �̻�� Session ����ü�� ��ȯ�Ѵ�.
/// </summary>
Session* Network::GetEmptySession()
{
	for (auto& session : session_list_)
	{
		if (session.socket_ == INVALID_SOCKET) {
			return &session;
		}
	}

	return nullptr;
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
	if (!CheckIOCPBindResult(handle)) {
		std::cout << "[BindIOCompletionPort] Failed With Error Code : " << GetLastError() << '\n';
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
	p_session->recv_overlapped_ex_.wsa_buf_.len = p_session->buf_size_;
	p_session->recv_overlapped_ex_.wsa_buf_.buf = p_session->recv_buf_;
	p_session->recv_overlapped_ex_.op_type_ = IOOperation::kRECV;

	// Recv ��û
	auto recv_ret = WSARecv(p_session->socket_,
							&p_session->recv_overlapped_ex_.wsa_buf_,
							1, &recved_bytes, &flag,
							&p_session->recv_overlapped_ex_.wsa_overlapped_,
							NULL);

	// ���� ó��
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
/// WSASend Overlapped I/O �۾��� ��û�Ѵ�.
/// </summary>
bool Network::SendMsg(Session* p_session, char* p_msg, uint32_t len)
{
	DWORD sent_bytes = 0;

	// p_msg -> overlappped ���۷� ������ �޽��� ����
	CopyMemory(p_session->send_buf_, p_msg, len);

	// ���� ���� �� I/O Operation Ÿ�� ����
	p_session->send_overlapped_ex_.wsa_buf_.len = len;
	p_session->send_overlapped_ex_.wsa_buf_.buf = p_session->send_buf_;
	p_session->send_overlapped_ex_.op_type_ = IOOperation::kSEND;

	// Send ��û
	auto send_ret = WSASend(p_session->socket_,
							&p_session->send_overlapped_ex_.wsa_buf_,
							1, &sent_bytes, 0,
							&p_session->send_overlapped_ex_.wsa_overlapped_,
							NULL);

	// ���� ó��
	if (send_ret == SOCKET_ERROR) {
		auto err = WSAGetLastError();

		if (err != ERROR_IO_PENDING) {
			std::cout << "[SendMsg] Failed With Error Code : " << err << '\n';
			return false;
		}
	}

	return true;
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
											 (PULONG_PTR)&p_session,
											 &p_overlapped, INFINITE);

		// ������ ���� �޽������� üũ
 		if (TerminateMsg(gqcs_ret, io_size, p_overlapped)) {
			std::cout << "[WorkerThread] Received Finish Message\n";

			// �ٸ� ������鵵 ����� �� �ֵ��� �޽��� ����
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
		OnDisconnect(p_session);

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
/// Overlapped�� I/O Ÿ�Կ� ���� Recv Ȥ�� Send �Ϸ� ��ƾ�� �����Ѵ�.
/// </summary>
void Network::DispatchOverlapped(Session* p_session, DWORD io_size, LPOVERLAPPED p_overlapped)
{
	// Ȯ�� Overlapped ����ü�� ĳ����
	OverlappedEx* p_overlapped_ex = reinterpret_cast<OverlappedEx*>(p_overlapped);

	// Recv �Ϸ� ����
	if (p_overlapped_ex->op_type_ == IOOperation::kRECV) {

		// Recv �Ϸ� �� �ٽ� Recv ��û�� �Ǵ�.
		std::cout << "[WorkerThread] Recv Completion\n";
		OnRecv(p_session, io_size);

		if (!BindRecv(p_session)) {

			// Recv ��û ���� �� ���� ����
			std::cout << "[WorkerThread] Failed to Re-bind Recv\n";
			CloseSocket(p_session);
		}

		return;
	}

	// Send �Ϸ� ����
	if (p_overlapped_ex->op_type_ == IOOperation::kSEND) {

		// Send �Ϸ�
		std::cout << "[WorkerThread] Send Completion : " << io_size << " Bytes\n";

		return;
	}
}

/// <summary>
/// ���� Accept �۾��� ó���ϴ� Accepter ������
/// </summary>
void Network::AccepterThread()
{
	while (is_accepter_running_)
	{
		// accept�� ���� �����͸� ���� ����ü ����
		SOCKADDR_IN accepted_addr;
		int32_t accepted_addr_len = sizeof(accepted_addr);

		// Max Session Count�� ������ �ʾҴٸ�, Session Ǯ���� �ϳ��� ������
		Session* p_accepted_session = GetEmptySession();
		if (p_accepted_session == NULL) {
			std::cout << "[AccepterThread] Current Session Count is MAX\n";
			return;
		}

		// Accept
		SOCKET accepted_socket = accept(listen_socket_,
										reinterpret_cast<SOCKADDR*>(&accepted_addr),
										&accepted_addr_len);

		// ���� ó��
		if (accepted_socket == INVALID_SOCKET) {
			auto err = WSAGetLastError();

			// ���� �޽����� �߻��Ͽ� listen_socket�� close�� ���
			if (err == WSAEINTR) {
				std::cout << "[AccepterThread] Accept Interrupted\n";
				return;
			}

			std::cout << "[AccepterThread] Failed With Error Code : " << WSAGetLastError() << '\n';
			continue;
		}

		p_accepted_session->socket_ = accepted_socket;
		std::cout << "[AccepterThread] New Session Accepted\n";

		// �Ϸ� ���� ��Ʈ�� ���ε�
		if (!BindIOCompletionPort(p_accepted_session))
		{
			return;
		}

		// Recv ��û
		if (!BindRecv(p_accepted_session))
		{
			return;
		}

		// Accept ���� ���� ����
		OnAccept(p_accepted_session);

		++session_cnt_;
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

	LINGER linger = { 0, 0 };

	// is_force�� true��� RST ó��
	if (is_force) {
		linger.l_onoff = 1;
	}

	// Linger �ɼ� ����
	setsockopt(p_session->socket_, SOL_SOCKET,
			   SO_LINGER, reinterpret_cast<const char*>(&linger), sizeof(linger));

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
	// Session ����ü �ʱ�ȭ
	ZeroMemory(&p_session->recv_overlapped_ex_, sizeof(OverlappedEx));
	ZeroMemory(&p_session->send_overlapped_ex_, sizeof(OverlappedEx));
	p_session->socket_ = INVALID_SOCKET;
}