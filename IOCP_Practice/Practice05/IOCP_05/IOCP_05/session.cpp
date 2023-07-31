#include "session.h"

#include <iostream>

/// <summary> <para>
/// WSASend�� Call�Ͽ� �񵿱� Send ��û </para> <para>
/// WSASend�� �������� ���� false�� �����Ѵ�. </para>
/// </summary>
bool Session::BindSend()
{
	// CAS ������ ���� �̹� Send��û�� ������ Ȯ���ϰ�, Send ��û�� ���۵����� Flag ������ ����
	bool sending_expected = false;
	if (!is_sending_.compare_exchange_strong(sending_expected, true)) {
		return true;
	}

	// Send ������ �� �ɱ�
	std::lock_guard<std::mutex> lock(send_lock_);

	DWORD sent_bytes = 0;

	// ����, ���� ���� �� I/O Operation Ÿ�� ����
	send_overlapped_ex_.socket_ = socket_;
	send_overlapped_ex_.op_type_ = IOOperation::kSEND;

	// WSABUF ���� : Send �����ۿ� �°� ����
	WSABUF wsa_buf[2];
	int32_t buffer_cnt = 1;
	SetWsaBuf(wsa_buf, buffer_cnt);

	// Send ��û
	auto send_ret = WSASend(socket_,
							wsa_buf,
							buffer_cnt, &sent_bytes, 0,
							reinterpret_cast<LPWSAOVERLAPPED>(&send_overlapped_ex_),
							NULL);

	// ���� ó��
	if (send_ret == SOCKET_ERROR) {
		auto err = WSAGetLastError();

		if (!AcceptableErrorCode(err)) {
			std::cout << "[SendMsg] Failed With Error Code : " << err << '\n';
			return false;
		}
	}

	// Send ��û �Ϸ�� ��ŭ �����ۿ��� ������ ����
	send_buf_.MoveFront(sent_bytes);

	return true;
}

/// <summary>
/// Send �� ���ۿ� �ִ� ������ ũ�⿡ ���� WSABUF�� �����Ѵ�.
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
/// ���� �ɰ� ������ Send �� ���ۿ� �����͸� �ִ´�.
/// </summary>
int32_t Session::EnqueueSendData(char* data, int32_t len)
{
	// Send ������ �� �ɱ�
	std::lock_guard<std::mutex> lock(send_lock_);

	// Enqueue�� ������ ũ�⸦ ����
	return send_buf_.Enqueue(data, len);
}

/// <summary>
/// Send �� ���ۿ� ���� �����Ͱ� �ִ��� ���θ� ��ȯ�Ѵ�.
/// </summary>
bool Session::HasSendData()
{
	// Send ������ �� �ɱ�
	std::lock_guard<std::mutex> lock(send_lock_);

	// Send �����ۿ� �����Ͱ� �ִ��� Ȯ��
	return send_buf_.GetSizeInUse() > 0;
}