#pragma once

#include <cstdint>

#include "overlapped_ex_define.h"

#define DEFAULT_BUF_SIZE 1024

// ���� ���� ����
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

	int32_t			index_ = -1;					// ���� �ε���
	SOCKET			socket_ = INVALID_SOCKET;		// Ŭ���̾�Ʈ ����
	int32_t			buf_size_ = DEFAULT_BUF_SIZE;	// ���� ũ��
	char*			recv_buf_ = nullptr;			// ���� ������ ����
	OverlappedEx	recv_overlapped_ex_;			// Recv I/O�� ���� OverlappedEx ����ü
};