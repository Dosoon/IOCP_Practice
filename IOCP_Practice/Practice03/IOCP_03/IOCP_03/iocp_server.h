#pragma once
#pragma comment(lib, "ws2_32")
#include <WinSock2.h>
#include <WS2tcpip.h>

#include <thread>
#include <vector>

#include "define_struct.h"

class IOCPServer
{
public:
	IOCPServer()
	{
	}
	~IOCPServer()
	{
		WSACleanup();
	}

	/// <summary>
	/// WSAStartup �� �������� �ʱ�ȭ
	/// </summary>
	bool InitSocket();

	/// <summary> <para>
	/// ���� �ּ� ������ ���� ���Ͽ� Bind�ϰ� Listen ó�� </para> <para>
	/// ��α� ť ����� �������� ������ 5�� �����ȴ�. </para>
	/// </summary>
	bool BindAndListen(const uint16_t port, const int32_t backlog_queue_size = 5);

	/// <summary>
	/// ClientInfo Ǯ ����, IOCP ����, ��Ŀ������ ����, Accepter ������ ����
	/// </summary>
	bool StartServer(const uint32_t max_client_count);

	/// <summary>
	/// ������ WorkerThread�� AccepterThread�� ��� �ı��Ѵ�.
	/// </summary>
	void DestroyThread();

	/// <summary>
	/// ������ ���������� ��ȯ�Ѵ�.
	/// </summary>
	bool IsServerRunning()
	{
		return is_server_running_;
	}

	/// <summary>
	/// ��� �����带 �ı��ϰ� ������ �����Ѵ�.
	/// </summary>
	void PostTerminateMsg()
	{
		PostQueuedCompletionStatus(iocp_, 0, NULL, NULL);
	}

	/// <summary>
	/// Accept �۾��� ���� �� ȣ��ȴ�.
	/// </summary>
	virtual void OnAccept(ClientInfo* p_client_info) = 0;

	/// <summary>
	/// Recv I/O �۾��� ���� �� ȣ��ȴ�.
	/// </summary>
	virtual void OnRecv(ClientInfo* p_client_info, DWORD io_size) = 0;

	/// <summary>
	/// Send I/O �۾��� ���� �� ȣ��ȴ�.
	/// </summary>
	virtual void OnDisconnect(ClientInfo* p_client_info) = 0;

protected:
	/// <summary>
	/// WSASend Overlapped I/O �۾��� ��û�Ѵ�.
	/// </summary>
	bool SendMsg(ClientInfo* p_client_info, char* p_msg, uint32_t len);

private:
	/// <summary>
	/// ClientInfo Ǯ�� ClientInfo���� �����Ѵ�.
	/// </summary>
	void CreateClient(const uint32_t max_client_count);

	/// <summary>
	/// WorkerThread Ǯ�� WorkerThread���� �����Ѵ�.
	/// </summary>
	bool CreateWorkerThread();

	/// <summary>
	/// AccepterThread�� �����Ѵ�.
	/// </summary>
	bool CreateAccepterThread();

	/// <summary>
	/// ClientInfo Ǯ���� �̻�� ClientInfo ����ü�� ��ȯ�Ѵ�.
	/// </summary>
	ClientInfo* GetEmptyClientInfo();

	/// <summary>
	/// IOCP ��ü�� Ŭ���̾�Ʈ ���� �� CompletionKey�� �����Ѵ�.
	/// </summary>
	bool BindIOCompletionPort(ClientInfo* p_client_info);

	/// <summary>
	/// WSARecv Overlapped I/O �۾��� ��û�Ѵ�.
	/// </summary>
	bool BindRecv(ClientInfo* p_client_info);

	/// <summary>
	/// Overlapped I/O �۾� �Ϸ� ������ ó���ϴ� ��Ŀ ������
	/// </summary>
	void WorkerThread();

	/// <summary>
	/// ���� Accept �۾��� ó���ϴ� Accepter ������
	/// </summary>
	void AccepterThread();

	/// <summary> <para>
	/// ���� ������ �����Ų��. </para><para>
	/// is_force�� true��� RST ó���Ѵ�. </para>
	/// </summary>
	void CloseSocket(ClientInfo* p_client_info, bool is_force = false);

	/// <summary>
	/// GetQueuedCompletionStatus ����� ���� ���� ó�� ���θ� ��ȯ�Ѵ�.
	/// </summary>
	bool CheckGQCSResult(ClientInfo* p_client_info, bool gqcs_ret, DWORD io_size, LPOVERLAPPED p_overlapped);

	/// <summary>
	/// Overlapped�� I/O Ÿ�Կ� ���� Recv Ȥ�� Send �Ϸ� ��ƾ�� �����Ѵ�.
	/// </summary>
	void DispatchOverlapped(ClientInfo* p_client_info, DWORD io_size, LPOVERLAPPED p_overlapped);

	/// <summary>
	/// ������ ���� ó�� �޽������� Ȯ���Ѵ�.
	/// </summary>
	bool TerminateMsg(DWORD io_size, LPOVERLAPPED p_overlapped)
	{
		return (io_size == 0 && p_overlapped == NULL);
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

	/// <summary>
	/// ClientInfo ����ü�� �����ϱ� ���� �ʱ�ȭ�Ѵ�.
	/// </summary>
	void ClearClientInfo(ClientInfo* p_client_info);

	/// <summary> <para>
	/// �ھ� ���� ������� Max Worker Thread ������ ����� ��ȯ�մϴ�. </para> <para>
	/// ���� ��� ��� : �ھ� �� * 2 + 1 </para>
	/// </summary>
	int32_t GetMaxWorkerThread();

	bool						is_server_running_ = false;
	std::vector<ClientInfo>		client_info_list_;
	SOCKET						listen_socket_ = INVALID_SOCKET;
	uint32_t					client_cnt_ = 0;
	std::vector<std::thread>	worker_thread_list_;
	std::thread					accepter_thread_;
	HANDLE						iocp_ = INVALID_HANDLE_VALUE;
	bool						is_worker_running_ = true;
	bool						is_accepter_running_ = true;
	char						socket_buf_[1024];
};