#pragma once

#include <cstdint>
#include <mutex>
#include <queue>

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

	bool BindSend();
	void EnqueueSendData(char* data, int32_t data_len);

	/// <summary>
	/// ���� ������ ���Ǵ� ���� �ڵ����� �˻��Ѵ�.
	/// </summary>
	bool AcceptableErrorCode(int32_t errorCode)
	{
		return errorCode == ERROR_IO_PENDING;
	}

	std::atomic<bool>			is_activated_ = false;			// ���� ������ ��������� ����

	int32_t						index_ = -1;					// ���� �ε���
	SOCKET						socket_ = INVALID_SOCKET;		// Ŭ���̾�Ʈ ����
	int32_t						buf_size_ = DEFAULT_BUF_SIZE;	// ���� ũ��
	char*						recv_buf_ = nullptr;			// ���� ������ ����
	OverlappedEx				recv_overlapped_ex_;			// Recv I/O�� ���� OverlappedEx ����ü
	std::mutex					send_queue_lock_;				// �۽� ������ ť�� ���� ��
	std::queue<OverlappedEx*>	send_data_queue_;				// �۽� �����͸� ���������� �����ϱ� ���� ť
};