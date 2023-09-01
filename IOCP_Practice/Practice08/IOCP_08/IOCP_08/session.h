#pragma once
#pragma comment(lib, "ws2_32")
#pragma comment(lib, "mswsock")
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <MSWSock.h>

#include <cstdint>
#include <mutex>

#include "overlapped_ex_define.h"
#include "ring_buffer.h"

#define DEFAULT_BUF_SIZE 1024

// 접속 세션 정보
class Session
{
public:
	Session() : recv_overlapped_ex_() {

	};

	Session(int32_t index, int32_t buf_size) {
		index_ = index;
		buf_size_ = buf_size;

		ZeroMemory(&accept_overlapped_ex_, sizeof(OverlappedEx));
		ZeroMemory(&recv_overlapped_ex_, sizeof(OverlappedEx));
		ZeroMemory(&send_overlapped_ex_, sizeof(OverlappedEx));
		socket_ = INVALID_SOCKET;

		recv_buf_ = new char[buf_size_];
	}

	~Session() {
		delete[] recv_buf_;
	}

	void Activate();

	// ------------------------------
	// Accept
	// ------------------------------

	bool BindAccept(SOCKET listen_socket);
	void SetAcceptOverlapped();
	bool GetSessionIpPort(char* ip_dest, int32_t ip_len, uint16_t& port_dest);

	// ------------------------------
	// Send & Recv
	// ------------------------------

	bool BindSend();
	void SetSendOverlapped(WSABUF* wsa_buf, int32_t& buffer_cnt);
	void SetWsaBuf(WSABUF* wsa_buf, int32_t& buffer_cnt);

	bool BindRecv();
	void SetRecvOverlapped();

	// ------------------------------
	// Send 버퍼 접근
	// ------------------------------

	int32_t EnqueueSendData(char* data_, int32_t data_len);
	void ClearSendBuffer();
	bool HasSendData();

	// ------------------------------
	// Getter & Setter
	// ------------------------------
	
	void SetConnectionClosedTime(uint64_t time)
	{
		latest_conn_closed_ = time;
	}

	uint64_t GetConnectionClosedTime()
	{
		return latest_conn_closed_;
	}

	void SetSocket(SOCKET sock)
	{
		socket_ = sock;
	}

	SOCKET GetSocket()
	{
		return socket_;
	}

	// ------------------------------
	// 에러 핸들링
	// ------------------------------

	bool ErrorHandler(bool result, int32_t error_code, const char* method,
					  int32_t allow_codes, ...);
	bool ErrorHandler(int32_t socket_result, int32_t error_code, const char* method,
					  int32_t allow_codes, ...);



	int32_t				index_ = -1;					// 세션 인덱스
	SOCKET				socket_ = INVALID_SOCKET;		// 클라이언트 소켓
	char*				recv_buf_ = nullptr;			// 수신 데이터 버퍼
	RingBuffer			send_buf_;						// 송신 데이터용 링 버퍼
	int32_t				buf_size_ = DEFAULT_BUF_SIZE;	// 버퍼 크기

	std::atomic<bool>	is_activated_ = false;			// 현재 세션이 사용중인지 여부
	std::atomic<bool>	is_sending_ = false;			// 현재 송신 중인지 여부
	std::mutex			send_lock_;						// 송신 데이터에 대한 락

	OverlappedEx		accept_overlapped_ex_;			// Accept I/O를 위한 OverlappedEx 구조체
	OverlappedEx		recv_overlapped_ex_;			// Recv I/O를 위한 OverlappedEx 구조체
	OverlappedEx		send_overlapped_ex_;			// Send I/O를 위한 OverlappedEx 구조체

	char				accept_buf_[64] = {0, };		// 원격지 정보를 가져오기 위한 버퍼
	uint64_t			latest_conn_closed_ = 0;		// 가장 최근에 연결이 끊긴 시각
};