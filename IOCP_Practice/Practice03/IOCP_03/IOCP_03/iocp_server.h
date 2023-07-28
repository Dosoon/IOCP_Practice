#pragma once
#pragma comment(lib, "ws2_32")
#include <WinSock2.h>
#include <WS2tcpip.h>

#include <thread>
#include <vector>

#include "define.h"

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

	bool InitSocket();
	bool BindAndListen(const uint16_t port, const int32_t backlog_queue_size = 5);
	bool CreateThread();
	bool StartServer(const uint32_t max_client_count);
	void Terminate();
	void DestroyThread();
	bool IsServerRunning()
	{
		return is_server_running_;
	}
	void PostTerminateMsg()
	{
		PostQueuedCompletionStatus(iocp_, 0, NULL, NULL);
	}

	virtual void OnAccept(ClientInfo* p_client_info) = 0;
	virtual void OnRecv(ClientInfo* p_client_info, DWORD io_size) = 0;
	virtual void OnDisconnect(ClientInfo* p_client_info) = 0;


protected:
	bool SendMsg(ClientInfo* p_client_info, char* p_msg, uint32_t len);

private:
	void CreateClientPool(const uint32_t max_client_count);
	bool CreateWorkerThread();
	bool CreateAccepterThread();

	ClientInfo* GetEmptyClientInfo();
	bool BindIOCompletionPort(ClientInfo* p_client_info);
	bool BindRecv(ClientInfo* p_client_info);

	void WorkerThread();
	void AccepterThread();

	void CloseSocket(ClientInfo* p_client_info, bool is_force = false);
	void ClearClientInfo(ClientInfo* p_client_info);

	void DispatchOverlapped(ClientInfo* p_client_info, DWORD io_size, LPOVERLAPPED p_overlapped);
	bool CheckGQCSResult(ClientInfo* p_client_info, bool gqcs_ret, DWORD io_size, LPOVERLAPPED p_overlapped);

	bool DestroyWorkerThread();
	bool DestroyAccepterThread();

	/// <summary>
	/// 쓰레드 종료 처리 메시지인지 확인한다.
	/// </summary>
	bool TerminateMsg(bool gqcs_ret, DWORD io_size, LPOVERLAPPED p_overlapped)
	{
		return (gqcs_ret == true && io_size == 0 && p_overlapped == NULL);
	}

	/// <summary>
	/// 클라이언트가 접속 종료했는지 확인한다.
	/// </summary>
	bool ClientExited(bool gqcs_ret, DWORD io_size, LPOVERLAPPED p_overlapped)
	{
		return (gqcs_ret == false || (p_overlapped != NULL && io_size == 0));
	}

	/// <summary>
	/// 소켓 디스크립터를 IOCP에 바인딩한 결과가 성공인지 여부를 반환한다.
	/// </summary>
	bool CheckIOCPBindResult(HANDLE handle)
	{
		return (handle != NULL && handle == iocp_);
	}

	/// <summary> <para>
	/// 코어 수를 기반으로 Max Worker Thread 개수를 계산해 반환합니다. </para> <para>
	/// 현재 계산 방식 : 코어 수 * 2 + 1 </para>
	/// </summary>
	int32_t GetMaxWorkerThread()
	{
		return std::thread::hardware_concurrency() * 2 + 1;
	}

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