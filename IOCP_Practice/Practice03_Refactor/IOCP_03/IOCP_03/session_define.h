#pragma once

#include <cstdint>

#include "overlapped_ex_define.h"

// ���� ���� ����
class Session
{
public:
	Session() {

	};

	Session(int32_t buf_size) {
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

	int32_t			index_;						// ���� �ε���
	SOCKET			socket_;					// Ŭ���̾�Ʈ ����
	int32_t			buf_size_;					// ���� ũ��
	char*			recv_buf_;					// ���� ������ ����
	char*			send_buf_;					// �۽� ������ ����
	OverlappedEx	recv_overlapped_ex_;		// Recv I/O�� ���� OverlappedEx ����ü
	OverlappedEx	send_overlapped_ex_;		// Send I/O�� ���� OverlappedEx ����ü
};