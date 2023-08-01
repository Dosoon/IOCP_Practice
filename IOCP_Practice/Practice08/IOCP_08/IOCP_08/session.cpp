#include "session.h"

#include <iostream>

/// <summary>
/// AcceptEx �񵿱� ��û
/// </summary>
bool Session::BindAccept(SOCKET listen_socket)
{
	// �ߺ� ��û ������ ���� time�� ����
	latest_conn_closed_ = UINT64_MAX;

	// ���� ����
	socket_ = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_IP, NULL, 0, WSA_FLAG_OVERLAPPED);
	if (socket_ == INVALID_SOCKET) {
		return false;
	}

	// Accept OverlappedEx �ʱ�ȭ
	InitAcceptOverlapped();

	// AcceptEx
	DWORD bytes_recv = 0;
	auto accept_ret = AcceptEx(listen_socket, socket_, accept_buf_, 0, sizeof(SOCKADDR_IN) + 16,
		sizeof(SOCKADDR_IN) + 16, &bytes_recv, reinterpret_cast<LPOVERLAPPED>(&accept_overlapped_ex_));

	return ErrorHandler(accept_ret, WSAGetLastError(), "AcceptEx", 1, WSA_IO_PENDING);
}

/// <summary>
/// Accept OverlappedEx �ʱ�ȭ
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
	auto send_ret = WSASend(socket_, wsa_buf, buffer_cnt, &sent_bytes, 0,
		reinterpret_cast<LPWSAOVERLAPPED>(&send_overlapped_ex_), NULL);

	// ���� ó��
	if (!ErrorHandler(send_ret, WSAGetLastError(), "WSASend", 1, ERROR_IO_PENDING)) {
		return false;
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

/// <summary> <para>
/// ���Ǵ� ���� �ڵ尡 �ƴ϶�� �α׸� �����. </para> <para>
/// �α׸� ����ٸ� false�� �����Ѵ�. </para> <para>
/// allow_codes�� ù ��° ���ڴ� ���Ǵ� ���� �ڵ��� �����̴�. </para>
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

/// <summary>
/// ���� �����͸� ���� IP �ּҿ� ��Ʈ ��ȣ�� �����´�.
/// </summary>
bool Session::GetSessionIpPort(char* ip_dest, int32_t ip_len, uint16_t& port_dest)
{
	// Peer ���� ��������
	SOCKADDR_IN* local_addr = NULL, * session_addr = NULL;
	int32_t session_addr_len = sizeof(session_addr);

	GetAcceptExSockaddrs(accept_buf_, 0, sizeof(SOCKADDR_IN) + 16, sizeof(SOCKADDR_IN) + 16,
		reinterpret_cast<SOCKADDR**>(&local_addr), &session_addr_len,
		reinterpret_cast<SOCKADDR**>(&session_addr), &session_addr_len);

	// IP �ּ� ���ڿ��� ��ȯ
	inet_ntop(AF_INET, &session_addr->sin_addr, ip_dest, ip_len);

	// ��Ʈ ����
	port_dest = session_addr->sin_port;

	return true;
}