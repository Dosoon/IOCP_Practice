#pragma once

#include <WinSock2.h>
#include <WS2tcpip.h>

#define max_sock_buf 1024		// 패킷 크기
#define max_worker_thread 4		// 쓰레드 풀에 넣을 워커 쓰레드 개수

enum class IOOperation
{
	kRECV,
	kSEND
};

// Overlapped 확장 구조체
struct OverlappedEx
{
	WSAOVERLAPPED	wsa_overlapped_;			// Overlapped I/O 구조체
	SOCKET			client_socket_;				// 클라이언트 소켓
	WSABUF			wsa_buf_;					// Overlapped I/O 작업 버퍼
	IOOperation		op_type_;					// Overlapped I/O 작업 타입
};

// 클라이언트 정보 구조체
struct ClientInfo
{
	SOCKET			client_socket_;				// 클라이언트 소켓
	char			recv_buf_[max_sock_buf];	// 수신 데이터 버퍼
	char			send_buf_[max_sock_buf];	// 송신 데이터 버퍼
	OverlappedEx	recv_overlapped_ex_;		// Recv I/O를 위한 OverlappedEx 구조체
	OverlappedEx	send_overlapped_ex_;		// Send I/O를 위한 OverlappedEx 구조체

	ClientInfo()
	{
		ZeroMemory(&recv_overlapped_ex_, sizeof(OverlappedEx));
		ZeroMemory(&send_overlapped_ex_, sizeof(OverlappedEx));
		client_socket_ = INVALID_SOCKET;
	}
};