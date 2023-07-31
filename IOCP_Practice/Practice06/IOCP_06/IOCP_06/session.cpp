#include "session.h"

#include <iostream>

/// <summary> <para>
/// WSASend를 Call하여 비동기 Send 요청 </para> <para>
/// WSASend가 실패했을 때에 false를 리턴한다. </para>
/// </summary>
bool Session::BindSend()
{
	DWORD sent_bytes = 0;

	// 큐의 front에 있는 OverlappedEx 로드
	auto send_overlapped_ex = send_data_queue_.front();

	// Send 요청
	auto send_ret = WSASend(socket_,
							&send_overlapped_ex->wsa_buf_,
							1, &sent_bytes, 0,
							reinterpret_cast<LPWSAOVERLAPPED>(send_overlapped_ex),
							NULL);

	// 에러 처리
	if (send_ret == SOCKET_ERROR) {
		auto err = WSAGetLastError();

		if (!AcceptableErrorCode(err)) {
			std::cout << "[SendMsg] Failed With Error Code : " << err << '\n';
			return false;
		}
	}

	return true;
}

/// <summary>
/// 락을 걸고 세션의 Send 데이터 큐에 데이터를 넣는다.
/// </summary>
void Session::EnqueueSendData(char* data, int32_t len)
{
	// Send 데이터 큐에 락 걸기
	std::lock_guard<std::mutex> lock(send_queue_lock_);

	// OverlappedEx 할당 및 Operation Type, 소켓 설정
	OverlappedEx* send_overlapped_ex = new OverlappedEx();
	send_overlapped_ex->op_type_ = IOOperation::kSEND;
	send_overlapped_ex->socket_ = socket_;

	// OverlappedEx 내부 WSABUF에 에코 데이터 할당
	send_overlapped_ex->wsa_buf_.buf = new char[len];
	send_overlapped_ex->wsa_buf_.len = len;
	CopyMemory(send_overlapped_ex->wsa_buf_.buf, data, len);

	// OverlappedEx 데이터를 Send 큐에 Enqueue
	send_data_queue_.push(send_overlapped_ex);

	if (send_data_queue_.size() == 1) {
		BindSend();
	}
}