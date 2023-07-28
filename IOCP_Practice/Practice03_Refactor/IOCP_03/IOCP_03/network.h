#pragma once
#pragma comment(lib, "ws2_32")
#include <WinSock2.h>
#include <WS2tcpip.h>

#include <thread>
#include <vector>
#include <functional>
#include "session_define.h"

class Network
{
public:
	Network() { }
	~Network()
	{
		WSACleanup();
	}

	bool InitSocket();
	bool BindAndListen(const uint16_t port, const int32_t backlog_queue_size = 5);
	bool CreateThread();
	bool StartNetwork(const uint32_t max_client_count);
	void Terminate();
	void DestroyThread();
	bool SendMsg(Session* p_client_info, char* p_msg, uint32_t len);
	void PostTerminateMsg()
	{
		PostQueuedCompletionStatus(iocp_, 0, NULL, NULL);
	}

	void SetOnAccept(std::function<void(Session*)> on_accept)
	{
		OnAccept = on_accept;
	}

	void SetOnRecv(std::function<void(Session*, DWORD)> on_recv)
	{
		OnRecv = on_recv;
	}

	void SetOnDisconnect(std::function<void(Session*)> on_disconnect)
	{
		OnDisconnect = on_disconnect;
	}

private:
	void CreateClientPool(const uint32_t max_client_count);
	bool CreateWorkerThread();
	bool CreateAccepterThread();

	Session* GetEmptyClientInfo();
	bool BindIOCompletionPort(Session* p_client_info);
	bool BindRecv(Session* p_client_info);

	void WorkerThread();
	void AccepterThread();

	void CloseSocket(Session* p_client_info, bool is_force = false);
	void ClearClientInfo(Session* p_client_info);

	void DispatchOverlapped(Session* p_client_info, DWORD io_size, LPOVERLAPPED p_overlapped);
	bool CheckGQCSResult(Session* p_client_info, bool gqcs_ret, DWORD io_size, LPOVERLAPPED p_overlapped);

	bool DestroyWorkerThread();
	bool DestroyAccepterThread();

	/// <summary>
	/// ������ ���� ó�� �޽������� Ȯ���Ѵ�.
	/// </summary>
	bool TerminateMsg(bool gqcs_ret, DWORD io_size, LPOVERLAPPED p_overlapped)
	{
		return (gqcs_ret == true && io_size == 0 && p_overlapped == NULL);
	}

	/// <summary>
	/// Ŭ���̾�Ʈ�� ���� �����ߴ��� Ȯ���Ѵ�.
	/// </summary>
	bool ClientExited(bool gqcs_ret, DWORD io_size, LPOVERLAPPED p_overlapped)
	{
		return (gqcs_ret == false || (p_overlapped != NULL && io_size == 0));
	}

	/// <summary>
	/// ���� ��ũ���͸� IOCP�� ���ε��� ����� �������� ���θ� ��ȯ�Ѵ�.
	/// </summary>
	bool CheckIOCPBindResult(HANDLE handle)
	{
		return (handle != NULL && handle == iocp_);
	}

	/// <summary> <para>
	/// �ھ� ���� ������� Max Worker Thread ������ ����� ��ȯ�մϴ�. </para> <para>
	/// ���� ��� ��� : �ھ� �� * 2 + 1 </para>
	/// </summary>
	int32_t GetMaxWorkerThread()
	{
		return std::thread::hardware_concurrency() * 2 + 1;
	}

	std::vector<Session>		client_info_list_;
	SOCKET						listen_socket_ = INVALID_SOCKET;
	uint32_t					client_cnt_ = 0;
	std::vector<std::thread>	worker_thread_list_;
	std::thread					accepter_thread_;
	HANDLE						iocp_ = INVALID_HANDLE_VALUE;
	bool						is_worker_running_ = true;
	bool						is_accepter_running_ = true;
	char						socket_buf_[1024];

	std::function<void(Session*)>			OnAccept = NULL;
	std::function<void(Session*, DWORD)>	OnRecv = NULL;
	std::function<void(Session*)>			OnDisconnect = NULL;
};