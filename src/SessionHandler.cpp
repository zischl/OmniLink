#include "SessionHandler.h"




int sessions::_init_winsock() {
	int wsResult;
	wsResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (wsResult != 0) {
		return 1;
	}
	return 0;
}

sockaddr_in sessions::_create_address(PCSTR IP, unsigned short port) {
	sockaddr_in address;
	address.sin_family = AF_INET;
	address.sin_port = htons(port);
	inet_pton(AF_INET, IP, &address.sin_addr);
	return address;
}

SOCKET sessions::_create_socket() {
	int wsResult;
	SOCKET socketR = INVALID_SOCKET;

	socketR = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (socketR == INVALID_SOCKET) {
		WSACleanup();
		return 1;
	}
	return socketR;


}




int sessions::_requestHandshake() {
	return 0;
}
int sessions::_verifyHandshake() {
	return 0;
}
int sessions::_establishLink() {
	return 0;
}
int  sessions::openSession() {
	return 0;
}

std::string _GetLocalAddr() {
	return "192.168.1.59";
}

void sessions::RegIOCP(SOCKET& socket) {
	if (IOCP == NULL) {
		ULONG_PTR CompletionKey = 0;
		IOCP = CreateIoCompletionPort((HANDLE*)socket, NULL, CompletionKey, 1);
		if (IOCP == NULL) {
			OutputDebugString("IOCP Port Creation Failed Successfully\n");
		}
	}

	std::thread StatusQueue([this]
		{
			while (true) {
				DWORD BufferSize = 0;
				ULONG_PTR EventKey = 0;
				OVERLAPPED* OVStruct = nullptr;

				bool WSResult = GetQueuedCompletionStatus(IOCP, &BufferSize, &EventKey, &OVStruct, INFINITE);
				if (!WSResult)
				{
					OutputDebugString("Completion Status Get False\n");
					if (OVStruct != nullptr) {
						OutputDebugString("Completion Status Get Failed\n");
						continue;
					}
					else {
						OutputDebugString("IOCP Thread Going Down...\n");
						break;
					}
				}
			}
		}
			);

	StatusQueue.detach();

}

void sessions::CreateConnection(PCSTR IP, unsigned short port) {

	address;
	address.sin_family = AF_INET;
	address.sin_port = htons(port);
	inet_pton(AF_INET, IP, &address.sin_addr);

	int wsResult;
	socketR = INVALID_SOCKET;

	socketR = WSASocket(AF_INET, SOCK_DGRAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
	if (socketR == INVALID_SOCKET) {
		WSACleanup();
	}


	/*int BindError;
	BindError = bind(socketR, (sockaddr*) &address, sizeof(address));
	if (BindError == 0) {
		OutputDebugString("Bind Failed Successfully\n");
	}*/



	RegIOCP(socketR);




}

void sessions::ChunkedSend(CHAR* data, int data_size, int MTU) {

	int MTU_slices = data_size - (data_size % MTU);

	for (int offset = 0; offset < MTU_slices; offset += MTU) {

		TransmitStruct* TrsBfrStruct = new TransmitStruct;

		TrsBfrStruct->TransmitBuffer = {};
		TrsBfrStruct->TransmitBuffer.buf = data + offset;
		TrsBfrStruct->TransmitBuffer.len = MTU;

		TrsBfrStruct->OVStruct = {};

		
		if (WSASendTo(socketR, &TrsBfrStruct->TransmitBuffer, 1, NULL, 0, (sockaddr*)&address, sizeof(address), &TrsBfrStruct->OVStruct, NULL) == SOCKET_ERROR) {
			int err = WSAGetLastError();
			if (err != ERROR_IO_PENDING)
			{
				OutputDebugString((std::to_string(err) + "\n").c_str());
			}
		}
	}

	TransmitStruct* TrsBfrStruct = new TransmitStruct;

	TrsBfrStruct->TransmitBuffer = {};
	TrsBfrStruct->OVStruct = {};
	
	TrsBfrStruct->TransmitBuffer.buf = data + MTU_slices;
	TrsBfrStruct->TransmitBuffer.len = data_size % MTU;

	TrsBfrStruct->OVStruct = {};

	WSASendTo(socketR, &TrsBfrStruct->TransmitBuffer, 1, NULL, 0, (sockaddr*)&address, sizeof(address), &TrsBfrStruct->OVStruct, NULL);

}