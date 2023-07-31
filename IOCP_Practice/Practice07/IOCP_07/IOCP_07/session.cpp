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
	ZeroMemory(&accept_overlapped_ex_, sizeof(OverlappedEx));
	accept_overlapped_ex_.session_idx_ = index_;
	accept_overlapped_ex_.socket_ = socket_;
	accept_overlapped_ex_.wsa_buf_.buf = NULL;
	accept_overlapped_ex_.wsa_buf_.len = 0;
	accept_overlapped_ex_.op_type_ = IOOperation::kACCEPT;

	// AcceptEx
	DWORD bytes_recv = 0;
	auto accept_ret = AcceptEx(listen_socket, socket_, accept_buf_, 0, sizeof(SOCKADDR_IN) + 16,
			 sizeof(SOCKADDR_IN) + 16, &bytes_recv, reinterpret_cast<LPOVERLAPPED>(&accept_overlapped_ex_));
	if (accept_ret == false) {
		auto err = WSAGetLastError();
		
		if (err != WSA_IO_PENDING) {
			std::cout << "[BindAccept] Failed With Error Code : " << err << '\n';
			return false;
		}
	}

	return true;
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
	auto send_ret = WSASend(socket_,
							wsa_buf,
							buffer_cnt, &sent_bytes, 0,
							reinterpret_cast<LPWSAOVERLAPPED>(&send_overlapped_ex_),
							NULL);

	// 에러 처리
	if (send_ret == SOCKET_ERROR) {
		auto err = WSAGetLastError();

		if (!AcceptableErrorCode(err)) {
			std::cout << "[SendMsg] Failed With Error Code : " << err << '\n';
			return false;
		}
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