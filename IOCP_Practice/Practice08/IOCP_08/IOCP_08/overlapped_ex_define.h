#pragma once
#include <WinSock2.h>
#include <WS2tcpip.h>

enum class IOOperation
{
	kNone,
	kACCEPT,
	kRECV,
	kSEND
};

// Overlapped Ȯ�� ����ü
struct OverlappedEx
{
	WSAOVERLAPPED	wsa_overlapped_;				// Overlapped I/O ����ü
	int32_t			session_idx_ = -1;				// ���� �ε���
	SOCKET			socket_;						// Ŭ���̾�Ʈ ����
	WSABUF			wsa_buf_;						// Overlapped I/O �۾� ����
	IOOperation		op_type_;						// Overlapped I/O �۾� Ÿ��

	OverlappedEx() {
		ZeroMemory(&wsa_overlapped_, sizeof(WSAOVERLAPPED));
		session_idx_ = -1;
		socket_ = INVALID_SOCKET;
		wsa_buf_ = { 0, };
		op_type_ = IOOperation::kNone;
	}
};