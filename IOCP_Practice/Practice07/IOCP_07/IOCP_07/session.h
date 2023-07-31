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

// ���� ���� ����
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
	/// ���� ������ ���Ǵ� ���� �ڵ����� �˻��Ѵ�.
	/// </summary>
	bool AcceptableErrorCode(int32_t errorCode)
	{
		return errorCode == ERROR_IO_PENDING;
	}

	std::atomic<bool>	is_activated_ = false;			// ���� ������ ��������� ����

	int32_t				index_ = -1;					// ���� �ε���
	SOCKET				socket_ = INVALID_SOCKET;		// Ŭ���̾�Ʈ ����
	int32_t				buf_size_ = DEFAULT_BUF_SIZE;	// ���� ũ��
	char*				recv_buf_ = nullptr;			// ���� ������ ����
	RingBuffer			send_buf_;						// �۽� �����Ϳ� �� ����
	OverlappedEx		accept_overlapped_ex_;			// Accept I/O�� ���� OverlappedEx ����ü
	OverlappedEx		recv_overlapped_ex_;			// Recv I/O�� ���� OverlappedEx ����ü
	OverlappedEx		send_overlapped_ex_;			// Send I/O�� ���� OverlappedEx ����ü
	std::atomic<bool>	is_sending_ = false;			// ���� �۽� ������ ����
	std::mutex			send_lock_;						// �۽� �����Ϳ� ���� ��
	uint64_t			latest_conn_closed_ = 0;		// ���� �ֱٿ� ������ ���� �ð�
	char				accept_buf_[64] = {0, };
};