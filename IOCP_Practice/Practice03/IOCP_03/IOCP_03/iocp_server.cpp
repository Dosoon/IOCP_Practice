#include "iocp_server.h"

#include <iostream>

bool IOCPServer::InitSocket()
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

bool IOCPServer::BindAndListen(const uint16_t port, int32_t backlog_queue_size)
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

bool IOCPServer::StartServer(const uint32_t max_client_count)
{
	is_server_running_ = true;

	// ClientInfo Ǯ ����
	CreateClient(max_client_count);

	// IOCP ����
	iocp_ = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
	if (iocp_ == NULL)
	{
		std::cout << "[CreateIoCompletionPort] Failed with Error Code : " << GetLastError() << '\n';
		return false;
	}

	// Worker ������ ����
	auto create_worker_ret = CreateWorkerThread();
	if (!create_worker_ret)
	{
		std::cout << "[CreateWorkerThread] Failed with Error Code : " << GetLastError() << '\n';
		return false;
	}

	// Accepter ������ ����
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
	// ��Ŀ ������ ����
	is_worker_running_ = false;

	for (auto& worker_thread : worker_thread_list_)
	{
		if (worker_thread.joinable()) {
			worker_thread.join();
		}
	}

	std::cout << "[DestroyThread] Worker Thread Destroyed\n";

	// Accepter ������ ����
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
	// reallocation ������ ���� reserve
	client_info_list_.reserve(max_client_count);

	// ClientInfo ����
	for (uint32_t i = 0; i < max_client_count; i++)
	{
		client_info_list_.emplace_back();
	}

	std::cout << "[CreateClient] OK\n";
}

bool IOCPServer::CreateWorkerThread()
{
	auto max_worker_thread = GetMaxWorkerThread();

	// reallocation ������ ���� reserve
	worker_thread_list_.reserve(max_worker_thread);

	// WorkerThread�� ����
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
	// Ŭ���̾�Ʈ ������ Completion Port�� ���ε�
	auto handle = CreateIoCompletionPort(reinterpret_cast<HANDLE>(p_client_info->client_socket_),
									     iocp_, reinterpret_cast<ULONG_PTR>(p_client_info), 0);

	// ���ε� �������� Ȯ��
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

	// ���� ���� �� I/O Operation Ÿ�� ����
	p_client_info->recv_overlapped_ex_.wsa_buf_.len = max_sock_buf;
	p_client_info->recv_overlapped_ex_.wsa_buf_.buf = p_client_info->recv_buf_;
	p_client_info->recv_overlapped_ex_.op_type_ = IOOperation::kRECV;

	// Recv ��û
	auto recv_ret = WSARecv(p_client_info->client_socket_,
							&p_client_info->recv_overlapped_ex_.wsa_buf_,
							1, &recved_bytes, &flag,
							&p_client_info->recv_overlapped_ex_.wsa_overlapped_,
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

bool IOCPServer::SendMsg(ClientInfo* p_client_info, char* p_msg, uint32_t len)
{
	DWORD sent_bytes = 0;

	// p_msg -> overlappped ���۷� ������ �޽��� ����
	CopyMemory(p_client_info->send_buf_, p_msg, len);

	// ���� ���� �� I/O Operation Ÿ�� ����
	p_client_info->send_overlapped_ex_.wsa_buf_.len = len;
	p_client_info->send_overlapped_ex_.wsa_buf_.buf = p_client_info->send_buf_;
	p_client_info->send_overlapped_ex_.op_type_ = IOOperation::kSEND;

	// Send ��û
	auto send_ret = WSASend(p_client_info->client_socket_,
							&p_client_info->send_overlapped_ex_.wsa_buf_,
							1, &sent_bytes, 0,
							&p_client_info->send_overlapped_ex_.wsa_overlapped_,
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

void IOCPServer::WorkerThread()
{
	// CompletionKey
	ClientInfo* p_client_info = NULL;

	// GQCS ��ȯ��
	bool gqcs_ret = false;

	// I/O ó���� ������ ������
	DWORD io_size = 0;

	// I/O �۾��� ���� ��û�� Overlapped ����ü�� ���� ������
	LPOVERLAPPED p_overlapped = NULL;

	while (is_worker_running_)
	{
		gqcs_ret = GetQueuedCompletionStatus(iocp_, &io_size,
											 (PULONG_PTR)&p_client_info,
											 &p_overlapped, INFINITE);

		// ������ ���� �޽������� üũ
 		if (TerminateMsg(io_size, p_overlapped)) {
			std::cout << "[WorkerThread] Received Finish Message\n";

			// �ٸ� ������鵵 ����� �� �ֵ��� �޽��� ����
			PostTerminateMsg();
			return;
		}

		// GQCS ����� ���� �Ϸ� �������� üũ
		if (!CheckGQCSResult(p_client_info, gqcs_ret, io_size, p_overlapped)) {
			continue;
		}

		// ���� �Ϸ� ������ ���� ó��
		DispatchOverlapped(p_client_info, io_size, p_overlapped);
	}
}

bool IOCPServer::CheckGQCSResult(ClientInfo* p_client_info, bool gqcs_ret, DWORD io_size, LPOVERLAPPED p_overlapped)
{
	// Ŭ���̾�Ʈ ���� ����
	if (ClientExited(gqcs_ret, io_size, p_overlapped)) {

		// ���� ���� ���� ����Ǿ�� �� ����
		OnDisconnect(p_client_info);

		// ���� �ݰ� ClientInfo �ʱ�ȭ
		CloseSocket(p_client_info);

		std::cout << "[WorkerThread] Client Exited\n";
		return false;
	}

	// IOCP ����
	if (p_overlapped == NULL) {
		std::cout << "[WorkerThread] Null Overlapped\n";
		return false;
	}

	return true;
}


void IOCPServer::DispatchOverlapped(ClientInfo* p_client_info, DWORD io_size, LPOVERLAPPED p_overlapped)
{
	// Ȯ�� Overlapped ����ü�� ĳ����
	OverlappedEx* p_overlapped_ex = reinterpret_cast<OverlappedEx*>(p_overlapped);

	// Recv �Ϸ� ����
	if (p_overlapped_ex->op_type_ == IOOperation::kRECV) {

		// Recv �Ϸ� �� �ٽ� Recv ��û�� �Ǵ�.
		std::cout << "[WorkerThread] Recv Completion\n";
		OnRecv(p_client_info, io_size);

		if (!BindRecv(p_client_info)) {

			// Recv ��û ���� �� ���� ����
			std::cout << "[WorkerThread] Failed to Re-bind Recv\n";
			CloseSocket(p_client_info);
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

void IOCPServer::AccepterThread()
{
	while (is_accepter_running_)
	{
		// accept�� ���� �����͸� ���� ����ü ����
		SOCKADDR_IN accepted_addr;
		int32_t accepted_addr_len = sizeof(accepted_addr);

		// Max Client Count�� ������ �ʾҴٸ�, ClientInfo Ǯ���� �ϳ��� ������
		ClientInfo* p_accepted_client_info = GetEmptyClientInfo();
		if (p_accepted_client_info == NULL) {
			std::cout << "[AccepterThread] Current Client Count is MAX\n";
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

		p_accepted_client_info->client_socket_ = accepted_socket;
		std::cout << "[AccepterThread] New Client Accepted\n";

		// �Ϸ� ���� ��Ʈ�� ���ε�
		if (!BindIOCompletionPort(p_accepted_client_info))
		{
			return;
		}

		// Recv ��û
		if (!BindRecv(p_accepted_client_info))
		{
			return;
		}

		// Accept ���� ���� ����
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

	// is_force�� true��� RST ó��
	if (is_force) {
		linger.l_onoff = 1;
	}

	// Linger �ɼ� ����
	setsockopt(p_client_info->client_socket_, SOL_SOCKET,
			   SO_LINGER, reinterpret_cast<const char*>(&linger), sizeof(linger));

	// ���� �ݰ�, ClientInfo �ʱ�ȭ �� Ǯ�� �ݳ�
	closesocket(p_client_info->client_socket_);
	ClearClientInfo(p_client_info);

	--client_cnt_;
}

void IOCPServer::ClearClientInfo(ClientInfo* p_client_info)
{
	// ClientInfo ����ü �ʱ�ȭ
	ZeroMemory(&p_client_info->recv_overlapped_ex_, sizeof(OverlappedEx));
	ZeroMemory(&p_client_info->send_overlapped_ex_, sizeof(OverlappedEx));
	p_client_info->client_socket_ = INVALID_SOCKET;
}