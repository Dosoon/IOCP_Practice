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

// Overlapped 확장 구조체
struct OverlappedEx
{
	WSAOVERLAPPED	wsa_overlapped_;				// Overlapped I/O 구조체
	int32_t			session_idx_ = -1;				// 세션 인덱스
	SOCKET			socket_;						// 클라이언트 소켓
	WSABUF			wsa_buf_;						// Overlapped I/O 작업 버퍼
	IOOperation		op_type_;						// Overlapped I/O 작업 타입

	OverlappedEx() {
		ZeroMemory(&wsa_overlapped_, sizeof(WSAOVERLAPPED));
		session_idx_ = -1;
		socket_ = INVALID_SOCKET;
		wsa_buf_ = { 0, };
		op_type_ = IOOperation::kNone;
	}
};