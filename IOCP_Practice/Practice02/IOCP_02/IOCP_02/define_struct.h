#pragma once

#include <WinSock2.h>
#include <WS2tcpip.h>

#define max_sock_buf 1024		// ��Ŷ ũ��
#define max_worker_thread 4		// ������ Ǯ�� ���� ��Ŀ ������ ����

enum class IOOperation
{
	kRECV,
	kSEND
};

// Overlapped Ȯ�� ����ü
struct OverlappedEx
{
	WSAOVERLAPPED	wsa_overlapped_;			// Overlapped I/O ����ü
	SOCKET			client_socket_;				// Ŭ���̾�Ʈ ����
	WSABUF			wsa_buf_;					// Overlapped I/O �۾� ����
	IOOperation		op_type_;					// Overlapped I/O �۾� Ÿ��
};

// Ŭ���̾�Ʈ ���� ����ü
struct ClientInfo
{
	SOCKET			client_socket_;				// Ŭ���̾�Ʈ ����
	char			recv_buf_[max_sock_buf];	// ���� ������ ����
	char			send_buf_[max_sock_buf];	// �۽� ������ ����
	OverlappedEx	recv_overlapped_ex_;		// Recv I/O�� ���� OverlappedEx ����ü
	OverlappedEx	send_overlapped_ex_;		// Send I/O�� ���� OverlappedEx ����ü

	ClientInfo()
	{
		ZeroMemory(&recv_overlapped_ex_, sizeof(OverlappedEx));
		ZeroMemory(&send_overlapped_ex_, sizeof(OverlappedEx));
		client_socket_ = INVALID_SOCKET;
	}
};