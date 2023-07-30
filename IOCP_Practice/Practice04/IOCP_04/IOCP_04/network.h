#pragma once
#pragma comment(lib, "ws2_32")
#include <WinSock2.h>
#include <WS2tcpip.h>

#include <thread>
#include <vector>
#include <functional>
#include "session.h"

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
	bool StartNetwork(const uint32_t max_session_cnt, const int32_t session_buf_size);
	void Terminate();
	void DestroyThread();
	// TODO : 인덱스 기반으로 변경
	bool SendMsg(Session* p_session, char* p_msg, uint32_t len);
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
	void CreateSessionPool(const int32_t max_session_cnt, const int32_t session_buf_size);
	bool CreateWorkerThread();
	bool CreateAccepterThread();

	Session* GetEmptySession();
	bool BindIOCompletionPort(Session* p_session);
	bool BindRecv(Session* p_session);

	void WorkerThread();
	void AccepterThread();

	void CloseSocket(Session* p_session, bool is_force = false);
	void ClearSession(Session* p_session);

	void DispatchOverlapped(Session* p_session, DWORD io_size, LPOVERLAPPED p_overlapped);
	bool CheckGQCSResult(Session* p_session, bool gqcs_ret, DWORD io_size, LPOVERLAPPED p_overlapped);

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
	bool SessionExited(bool gqcs_ret, DWORD io_size, LPOVERLAPPED p_overlapped)
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

	std::vector<Session>		session_list_;
	SOCKET						listen_socket_ = INVALID_SOCKET;
	uint32_t					session_cnt_ = 0;
	std::vector<std::thread>	worker_thread_list_;
	std::thread					accepter_thread_;
	HANDLE						iocp_ = INVALID_HANDLE_VALUE;
	bool						is_worker_running_ = true;
	bool						is_accepter_running_ = true;

	std::function<void(Session*)>			OnAccept = NULL;
	std::function<void(Session*, DWORD)>	OnRecv = NULL;
	std::function<void(Session*)>			OnDisconnect = NULL;
};