#include "session.h"

#include <iostream>

/// <summary> <para>
/// WSASend�� Call�Ͽ� �񵿱� Send ��û </para> <para>
/// WSASend�� �������� ���� false�� �����Ѵ�. </para>
/// </summary>
bool Session::BindSend()
{
	DWORD sent_bytes = 0;

	// ť�� front�� �ִ� OverlappedEx �ε�
	auto send_overlapped_ex = send_data_queue_.front();

	// Send ��û
	auto send_ret = WSASend(socket_,
							&send_overlapped_ex->wsa_buf_,
							1, &sent_bytes, 0,
							reinterpret_cast<LPWSAOVERLAPPED>(send_overlapped_ex),
							NULL);

	// ���� ó��
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
/// ���� �ɰ� ������ Send ������ ť�� �����͸� �ִ´�.
/// </summary>
void Session::EnqueueSendData(char* data, int32_t len)
{
	// Send ������ ť�� �� �ɱ�
	std::lock_guard<std::mutex> lock(send_queue_lock_);

	// OverlappedEx �Ҵ� �� Operation Type, ���� ����
	OverlappedEx* send_overlapped_ex = new OverlappedEx();
	send_overlapped_ex->op_type_ = IOOperation::kSEND;
	send_overlapped_ex->socket_ = socket_;

	// OverlappedEx ���� WSABUF�� ���� ������ �Ҵ�
	send_overlapped_ex->wsa_buf_.buf = new char[len];
	send_overlapped_ex->wsa_buf_.len = len;
	CopyMemory(send_overlapped_ex->wsa_buf_.buf, data, len);

	// OverlappedEx �����͸� Send ť�� Enqueue
	send_data_queue_.push(send_overlapped_ex);

	if (send_data_queue_.size() == 1) {
		BindSend();
	}
}