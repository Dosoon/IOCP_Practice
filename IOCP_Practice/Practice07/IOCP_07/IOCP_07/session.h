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

		accept_overlapped_ex_.session_idx_ = index;
		recv_overlapped_ex_.session_idx_ = index;
		send_overlapped_ex_.session_idx_ = index;
	}

	~Session() {
		delete[] recv_buf_;
	}

	bool BindAccept(SOCKET listen_socket);
	bool BindSend();
	int32_t EnqueueSendData(char* data, int32_t data_len);
	bool HasSendData();
	void SetWsaBuf(WSABUF* wsa_buf, int32_t& buffer_cnt);

	/// <summary>
	/// 연결 유지가 허용되는 에러 코드인지 검사한다.
	/// </summary>
	bool AcceptableErrorCode(int32_t errorCode)
	{
		return errorCode == ERROR_IO_PENDING;
	}

	std::atomic<bool>	is_activated_ = false;			// 현재 세션이 사용중인지 여부

	int32_t				index_ = -1;					// 세션 인덱스
	SOCKET				socket_ = INVALID_SOCKET;		// 클라이언트 소켓
	int32_t				buf_size_ = DEFAULT_BUF_SIZE;	// 버퍼 크기
	char*				recv_buf_ = nullptr;			// 수신 데이터 버퍼
	RingBuffer			send_buf_;						// 송신 데이터용 링 버퍼
	OverlappedEx		accept_overlapped_ex_;			// Accept I/O를 위한 OverlappedEx 구조체
	OverlappedEx		recv_overlapped_ex_;			// Recv I/O를 위한 OverlappedEx 구조체
	OverlappedEx		send_overlapped_ex_;			// Send I/O를 위한 OverlappedEx 구조체
	std::atomic<bool>	is_sending_ = false;			// 현재 송신 중인지 여부
	std::mutex			send_lock_;						// 송신 데이터에 대한 락
	uint64_t			latest_conn_closed_ = 0;		// 가장 최근에 연결이 끊긴 시각
	char				accept_buf_[64] = {0, };
};