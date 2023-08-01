#pragma once

#include <thread>
#include <vector>
#include <functional>

#include "session.h"

#define SESSION_REUSE_TIME 5

class Network
{
public:
	Network() { }
	~Network()
	{
		WSACleanup();
	}

	bool Start(uint16_t port, const uint32_t max_session_cnt, const int32_t session_buf_size);
	void Terminate();

	/// <summary>
	/// ���� �ܿ��� ������ Accept ���� �ݹ� �Լ��� �����Ѵ�.
	/// </summary>
	void SetOnConnect(std::function<void(int32_t)> on_connect)
	{
		OnConnect = on_connect;
	}

	/// <summary>
	/// ���� �ܿ��� ������ Recv ���� �ݹ� �Լ��� �����Ѵ�.
	/// </summary>
	void SetOnRecv(std::function<void(int32_t, const char*, DWORD)> on_recv)
	{
		OnRecv = on_recv;
	}

	/// <summary>
	/// ���� �ܿ��� ������ Disconnect ���� �ݹ� �Լ��� �����Ѵ�.
	/// </summary>
	void SetOnDisconnect(std::function<void(int32_t)> on_disconnect)
	{
		OnDisconnect = on_disconnect;
	}

	/// <summary>
	/// ���� �ε����� ���� �����͸� ��ȯ�Ѵ�.
	/// </summary>
	Session* GetSessionByIdx(int32_t session_idx)
	{
		return session_list_[session_idx];
	}

private:
	bool InitSocket();
	bool BindAndListen(const uint16_t port, const int32_t backlog_queue_size = 5);
	bool CreateThread();
	bool CreateIOCP();
	void DestroyThread();

	void CreateSessionPool(const int32_t max_session_cnt, const int32_t session_buf_size);
	bool CreateWorkerThread();
	bool CreateAccepterThread();
	bool CreateSenderThread();

	bool BindIOCompletionPort(Session* p_session);
	bool BindRecv(Session* p_session);
	void SetRecvOverlappedEx(Session* p_session);

	void WorkerThread();
	void AccepterThread();
	void SenderThread();

	void CloseSocket(Session* p_session, bool is_force = false);
	void ClearSession(Session* p_session);

	void DispatchOverlapped(Session* p_session, DWORD io_size, LPOVERLAPPED p_overlapped);
	bool CheckGQCSResult(Session* p_session, bool gqcs_ret, DWORD io_size, LPOVERLAPPED p_overlapped);

	bool DestroyWorkerThread();
	bool DestroyAccepterThread();
	bool DestroySenderThread();

	bool ErrorHandler(bool result, int32_t error_code, const char* method, int32_t allow_codes, ...);
	bool ErrorHandler(int32_t socket_result, int32_t error_code, const char* method, int32_t allow_codes, ...);
	bool ErrorHandler(HANDLE handle_result, int32_t error_code, const char* method, HANDLE allow_handle);

	/// <summary>
	/// ������鿡�� ���� �޽����� ������.
	/// </summary>
	void PostTerminateMsg()
	{
		PostQueuedCompletionStatus(iocp_, 0, NULL, NULL);
	}

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
	bool SessionExited(bool gqcs_ret, DWORD io_size, LPOVERLAPPED p_overlapped)
	{
		// GQCS false -> Ŭ���̾�Ʈ ������ ����(graceful shutdown�� �ƴ� ���) Ȥ�� GQCS Ÿ�Ӿƿ�
		// GQCS true, IO Size 0 -> graceful shutdown
		return (gqcs_ret == false ||
			(p_overlapped != NULL && io_size == 0 && reinterpret_cast<OverlappedEx*>(p_overlapped)->op_type_ != IOOperation::kACCEPT));
	}

	/// <summary> <para>
	/// �ھ� ���� ������� Max Worker Thread ������ ����� ��ȯ�մϴ�. </para> <para>
	/// ���� ��� ��� : �ھ� �� * 2 + 1 </para>
	/// </summary>
	int32_t GetMaxWorkerThread()
	{
		return std::thread::hardware_concurrency() * 2 + 1;
	}

	/// <summary>
	/// ���� ������ ���Ǵ� ���� �ڵ����� �˻��Ѵ�.
	/// </summary>
	bool AcceptableErrorCode(int32_t errorCode)
	{
		return errorCode == ERROR_IO_PENDING;
	}

	std::vector<Session*>		session_list_;
	SOCKET						listen_socket_ = INVALID_SOCKET;
	uint32_t					session_cnt_ = 0;
	std::vector<std::thread>	worker_thread_list_;
	std::thread					accepter_thread_;
	std::thread					sender_thread_;
	HANDLE						iocp_ = INVALID_HANDLE_VALUE;
	bool						is_worker_running_ = true;
	bool						is_accepter_running_ = true;
	bool						is_sender_running_ = true;	

	std::function<void(int32_t)>							OnConnect = NULL;
	std::function<void(int32_t, const char*, DWORD)>		OnRecv = NULL;
	std::function<void(int32_t)>							OnDisconnect = NULL;
};