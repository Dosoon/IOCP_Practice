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
	/// WSAStartup 및 리슨소켓 초기화
	/// </summary>
	bool InitSocket();

	/// <summary> <para>
	/// 서버 주소 정보를 리슨 소켓에 Bind하고 Listen 처리 </para> <para>
	/// 백로그 큐 사이즈를 지정하지 않으면 5로 설정된다. </para>
	/// </summary>
	bool BindAndListen(const uint16_t port, const int32_t backlog_queue_size = 5);

	/// <summary>
	/// ClientInfo 풀 생성, IOCP 생성, 워커쓰레드 생성, Accepter 쓰레드 생성
	/// </summary>
	bool StartServer(const uint32_t max_client_count);

	/// <summary>
	/// 생성된 WorkerThread와 AccepterThread를 모두 파괴한다.
	/// </summary>
	void DestroyThread();

	/// <summary>
	/// 서버가 실행중인지 반환한다.
	/// </summary>
	bool IsServerRunning()
	{
		return is_server_running_;
	}

	/// <summary>
	/// 모든 쓰레드를 파괴하고 서버를 종료한다.
	/// </summary>
	void PostTerminateMsg()
	{
		PostQueuedCompletionStatus(iocp_, 0, NULL, NULL);
	}

	/// <summary>
	/// Accept 작업이 끝난 후 호출된다.
	/// </summary>
	virtual void OnAccept(ClientInfo* p_client_info) = 0;

	/// <summary>
	/// Recv I/O 작업이 끝난 후 호출된다.
	/// </summary>
	virtual void OnRecv(ClientInfo* p_client_info, DWORD io_size) = 0;

	/// <summary>
	/// Send I/O 작업이 끝난 후 호출된다.
	/// </summary>
	virtual void OnDisconnect(ClientInfo* p_client_info) = 0;

protected:
	/// <summary>
	/// WSASend Overlapped I/O 작업을 요청한다.
	/// </summary>
	bool SendMsg(ClientInfo* p_client_info, char* p_msg, uint32_t len);

private:
	/// <summary>
	/// ClientInfo 풀에 ClientInfo들을 생성한다.
	/// </summary>
	void CreateClient(const uint32_t max_client_count);

	/// <summary>
	/// WorkerThread 풀에 WorkerThread들을 생성한다.
	/// </summary>
	bool CreateWorkerThread();

	/// <summary>
	/// AccepterThread를 생성한다.
	/// </summary>
	bool CreateAccepterThread();

	/// <summary>
	/// ClientInfo 풀에서 미사용 ClientInfo 구조체를 반환한다.
	/// </summary>
	ClientInfo* GetEmptyClientInfo();

	/// <summary>
	/// IOCP 객체에 클라이언트 소켓 및 CompletionKey를 연결한다.
	/// </summary>
	bool BindIOCompletionPort(ClientInfo* p_client_info);

	/// <summary>
	/// WSARecv Overlapped I/O 작업을 요청한다.
	/// </summary>
	bool BindRecv(ClientInfo* p_client_info);

	/// <summary>
	/// Overlapped I/O 작업 완료 통지를 처리하는 워커 쓰레드
	/// </summary>
	void WorkerThread();

	/// <summary>
	/// 동기 Accept 작업을 처리하는 Accepter 쓰레드
	/// </summary>
	void AccepterThread();

	/// <summary> <para>
	/// 소켓 연결을 종료시킨다. </para><para>
	/// is_force가 true라면 RST 처리한다. </para>
	/// </summary>
	void CloseSocket(ClientInfo* p_client_info, bool is_force = false);

	/// <summary>
	/// GetQueuedCompletionStatus 결과에 따라 정상 처리 여부를 반환한다.
	/// </summary>
	bool CheckGQCSResult(ClientInfo* p_client_info, bool gqcs_ret, DWORD io_size, LPOVERLAPPED p_overlapped);

	/// <summary>
	/// Overlapped의 I/O 타입에 따라 Recv 혹은 Send 완료 루틴을 수행한다.
	/// </summary>
	void DispatchOverlapped(ClientInfo* p_client_info, DWORD io_size, LPOVERLAPPED p_overlapped);

	/// <summary>
	/// 쓰레드 종료 처리 메시지인지 확인한다.
	/// </summary>
	bool TerminateMsg(DWORD io_size, LPOVERLAPPED p_overlapped)
	{
		return (io_size == 0 && p_overlapped == NULL);
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

	/// <summary>
	/// ClientInfo 구조체를 재사용하기 위해 초기화한다.
	/// </summary>
	void ClearClientInfo(ClientInfo* p_client_info);

	/// <summary> <para>
	/// 코어 수를 기반으로 Max Worker Thread 개수를 계산해 반환합니다. </para> <para>
	/// 현재 계산 방식 : 코어 수 * 2 + 1 </para>
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