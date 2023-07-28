#pragma once
#include <WinSock2.h>
#include <WS2tcpip.h>

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