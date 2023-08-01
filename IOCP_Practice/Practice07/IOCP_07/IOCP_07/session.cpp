#include "session.h"

#include <iostream>

/// <summary>
/// AcceptEx 비동기 요청
/// </summary>
bool Session::BindAccept(SOCKET listen_socket)
{
	// 중복 요청 방지를 위한 time값 조정
	latest_conn_closed_ = UINT64_MAX;

	// 소켓 생성
	socket_ = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_IP, NULL, 0, WSA_FLAG_OVERLAPPED);
	if (socket_ == INVALID_SOCKET) {
		return false;
	}

	// Accept OverlappedEx 초기화
	InitAcceptOverlapped();

	// AcceptEx
	DWORD bytes_recv = 0;
	auto accept_ret = AcceptEx(listen_socket, socket_, accept_buf_, 0, sizeof(SOCKADDR_IN) + 16,
		sizeof(SOCKADDR_IN) + 16, &bytes_recv, reinterpret_cast<LPOVERLAPPED>(&accept_overlapped_ex_));

	return ErrorHandler(accept_ret, WSAGetLastError(), "AcceptEx", 1, WSA_IO_PENDING);
}

/// <summary>
/// Accept OverlappedEx 초기화
/// </summary>
void Session::InitAcceptOverlapped()
{
	ZeroMemory(&accept_overlapped_ex_, sizeof(OverlappedEx));
	accept_overlapped_ex_.session_idx_ = index_;
	accept_overlapped_ex_.socket_ = socket_;
	accept_overlapped_ex_.wsa_buf_.buf = NULL;
	accept_overlapped_ex_.wsa_buf_.len = 0;
	accept_overlapped_ex_.op_type_ = IOOperation::kACCEPT;
}

/// <summary> <para>
/// WSASend를 Call하여 비동기 Send 요청 </para> <para>
/// WSASend가 실패했을 때에 false를 리턴한다. </para>
/// </summary>
bool Session::BindSend()
{
	// CAS 연산을 통해 이미 Send요청이 갔는지 확인하고, Send 요청이 시작됐음을 Flag 변수로 남김
	bool sending_expected = false;
	if (!is_sending_.compare_exchange_strong(sending_expected, true)) {
		return true;
	}

	// Send 링버퍼 락 걸기
	std::lock_guard<std::mutex> lock(send_lock_);

	DWORD sent_bytes = 0;

	// 소켓, 버퍼 정보 및 I/O Operation 타입 설정
	send_overlapped_ex_.socket_ = socket_;
	send_overlapped_ex_.op_type_ = IOOperation::kSEND;

	// WSABUF 설정 : Send 링버퍼에 맞게 설정
	WSABUF wsa_buf[2];
	int32_t buffer_cnt = 1;
	SetWsaBuf(wsa_buf, buffer_cnt);

	// Send 요청
	auto send_ret = WSASend(socket_, wsa_buf, buffer_cnt, &sent_bytes, 0,
		reinterpret_cast<LPWSAOVERLAPPED>(&send_overlapped_ex_), NULL);

	// 에러 처리
	if (!ErrorHandler(send_ret, WSAGetLastError(), "WSASend", 1, ERROR_IO_PENDING)) {
		return false;
	}

	// Send 요청 완료된 만큼 링버퍼에서 데이터 제거
	send_buf_.MoveFront(sent_bytes);

	return true;
}

/// <summary>
/// Send 링 버퍼에 있는 데이터 크기에 따라 WSABUF를 설정한다.
/// </summary>
void Session::SetWsaBuf(WSABUF* wsa_buf, int32_t& buffer_cnt)
{
	wsa_buf[0].buf = send_buf_.GetFrontBufferPtr();
	wsa_buf[0].len = send_buf_.GetContinuousDequeueSize();

	if (send_buf_.GetSizeInUse() > send_buf_.GetContinuousDequeueSize()) {
		wsa_buf[1].buf = send_buf_.GetBufferPtr();
		wsa_buf[1].len = send_buf_.GetSizeInUse() - send_buf_.GetContinuousDequeueSize();
		++buffer_cnt;
	}
}

/// <summary> <para>
/// 허용되는 에러 코드가 아니라면 로그를 남긴다. </para> <para>
/// 로그를 남겼다면 false를 리턴한다. </para> <para>
/// allow_codes의 첫 번째 인자는 허용되는 에러 코드의 개수이다. </para>
/// </summary>
bool Session::ErrorHandler(bool result, int32_t error_code, const char* method, int32_t allow_codes, ...)
{
	if (result) {
		return true;
	}
	else {
		va_list code_list;
		va_start(code_list, allow_codes);

		for (auto i = 0; i < allow_codes; ++i) {
			if (error_code == va_arg(code_list, int32_t)) {
				return true;
			}
		}

		std::cout << '[' << method << "] Failed With Error Code : " << error_code << '\n';
		va_end(code_list);
		return false;
	}
}

bool Session::ErrorHandler(int32_t socket_result, int32_t error_code, const char* method, int32_t allow_codes, ...)
{
	if (socket_result != SOCKET_ERROR) {
		return true;
	}
	else {
		va_list code_list;
		va_start(code_list, allow_codes);

		for (auto i = 0; i < allow_codes; ++i) {
			if (error_code == va_arg(code_list, int32_t)) {
				return true;
			}
		}

		std::cout << '[' << method << "] Failed With Error Code : " << error_code << '\n';
		va_end(code_list);
		return true;
	}
}

/// <summary>
/// 락을 걸고 세션의 Send 링 버퍼에 데이터를 넣는다.
/// </summary>
int32_t Session::EnqueueSendData(char* data, int32_t len)
{
	// Send 링버퍼 락 걸기
	std::lock_guard<std::mutex> lock(send_lock_);

	// Enqueue에 성공한 크기를 리턴
	return send_buf_.Enqueue(data, len);
}

/// <summary>
/// Send 링 버퍼에 보낼 데이터가 있는지 여부를 반환한다.
/// </summary>
bool Session::HasSendData()
{
	// Send 링버퍼 락 걸기
	std::lock_guard<std::mutex> lock(send_lock_);

	// Send 링버퍼에 데이터가 있는지 확인
	return send_buf_.GetSizeInUse() > 0;
}

/// <summary>
/// 세션 데이터를 토대로 IP 주소와 포트 번호를 가져온다.
/// </summary>
bool Session::GetSessionIpPort(char* ip_dest, int32_t ip_len, uint16_t& port_dest)
{
	// Peer 정보 가져오기
	SOCKADDR_IN* local_addr = NULL, * session_addr = NULL;
	int32_t session_addr_len = sizeof(session_addr);

	GetAcceptExSockaddrs(accept_buf_, 0, sizeof(SOCKADDR_IN) + 16, sizeof(SOCKADDR_IN) + 16,
		reinterpret_cast<SOCKADDR**>(&local_addr), &session_addr_len,
		reinterpret_cast<SOCKADDR**>(&session_addr), &session_addr_len);

	// IP 주소 문자열로 변환
	inet_ntop(AF_INET, &session_addr->sin_addr, ip_dest, ip_len);

	// 포트 정보
	port_dest = session_addr->sin_port;

	return true;
}