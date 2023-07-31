#pragma once

#include <cstdint>
#include <mutex>
#include <queue>

#include "overlapped_ex_define.h"

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

		ZeroMemory(&recv_overlapped_ex_, sizeof(OverlappedEx));
		socket_ = INVALID_SOCKET;

		recv_buf_ = new char[buf_size_];
	}

	~Session() {
		delete[] recv_buf_;
	}

	bool BindSend();
	void EnqueueSendData(char* data, int32_t data_len);

	/// <summary>
	/// 연결 유지가 허용되는 에러 코드인지 검사한다.
	/// </summary>
	bool AcceptableErrorCode(int32_t errorCode)
	{
		return errorCode == ERROR_IO_PENDING;
	}

	std::atomic<bool>			is_activated_ = false;			// 현재 세션이 사용중인지 여부

	int32_t						index_ = -1;					// 세션 인덱스
	SOCKET						socket_ = INVALID_SOCKET;		// 클라이언트 소켓
	int32_t						buf_size_ = DEFAULT_BUF_SIZE;	// 버퍼 크기
	char*						recv_buf_ = nullptr;			// 수신 데이터 버퍼
	OverlappedEx				recv_overlapped_ex_;			// Recv I/O를 위한 OverlappedEx 구조체
	std::mutex					send_queue_lock_;				// 송신 데이터 큐에 대한 락
	std::queue<OverlappedEx*>	send_data_queue_;				// 송신 데이터를 순차적으로 저장하기 위한 큐
};