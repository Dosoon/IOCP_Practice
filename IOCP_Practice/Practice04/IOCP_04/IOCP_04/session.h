#pragma once

#include <cstdint>

#include "overlapped_ex_define.h"

#define DEFAULT_BUF_SIZE 1024

// 접속 세션 정보
class Session
{
public:
	Session() : recv_overlapped_ex_(), send_overlapped_ex_() {

	};

	Session(int32_t index, int32_t buf_size) {
		index_ = index;
		buf_size_ = buf_size;

		ZeroMemory(&recv_overlapped_ex_, sizeof(OverlappedEx));
		ZeroMemory(&send_overlapped_ex_, sizeof(OverlappedEx));
		socket_ = INVALID_SOCKET;

		recv_buf_ = new char[buf_size_];
		send_buf_ = new char[buf_size_];
	}

	~Session() {
		delete[] recv_buf_;
		delete[] send_buf_;
	}

	int32_t			index_ = -1;					// 세션 인덱스
	SOCKET			socket_ = INVALID_SOCKET;		// 클라이언트 소켓
	int32_t			buf_size_ = DEFAULT_BUF_SIZE;	// 버퍼 크기
	char*			recv_buf_ = nullptr;			// 수신 데이터 버퍼
	char*			send_buf_ = nullptr;			// 송신 데이터 버퍼
	OverlappedEx	recv_overlapped_ex_;			// Recv I/O를 위한 OverlappedEx 구조체
	OverlappedEx	send_overlapped_ex_;			// Send I/O를 위한 OverlappedEx 구조체
};