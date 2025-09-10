#include "SessionHandler.h"


sessions::sessions() 
{
	WinsockInit();

}

int sessions::WinsockInit() 
{
	int wsResult;
	wsResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (wsResult != 0) {
		return 1;
	}
	return 0;
}

sockaddr_in sessions::CreateAddress(PCSTR IP, unsigned short port) 
{
	sockaddr_in address;
	address.sin_family = AF_INET;
	address.sin_port = htons(port);
	inet_pton(AF_INET, IP, &address.sin_addr);
	return address;
}

SOCKET sessions::CreateSocket() {
	SOCKET socketR = INVALID_SOCKET;

	socketR = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (socketR == INVALID_SOCKET) {
		WSACleanup();
		return 1;
	}
	return socketR;


}


uint32_t sessions::GetLocals(uint8_t family)
{
	int WSResult;

	ULONG Flags = GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER | GAA_FLAG_INCLUDE_PREFIX;

	ULONG Family = family == 4 ? AF_INET : family == 6 ? AF_INET6 : AF_UNSPEC;

	ULONG locals_size = 15000;

	uint8_t Retries = 2;

	PIP_ADAPTER_ADDRESSES locals = NULL;

	unsigned int i = 0;

	PIP_ADAPTER_ADDRESSES IterAddress = NULL;
	PIP_ADAPTER_UNICAST_ADDRESS Unicast = NULL;
	uint32_t LocalIP{};

	do {

		locals = (IP_ADAPTER_ADDRESSES*)MEMALLOC(locals_size);
		if (locals == NULL) {
			return 0;
		}

		WSResult = GetAdaptersAddresses(Family, Flags, NULL, locals, &locals_size);
		if (WSResult != ERROR_BUFFER_OVERFLOW && WSResult != ERROR_SUCCESS) {
			Logger::log("Error {}: Could Not Retrieve Local Addresses !\n", WSResult);
			FREE(locals);
		}

		Retries--;

	} while (WSResult != ERROR_SUCCESS && Retries != 0);

	if (WSResult == ERROR_SUCCESS && locals != NULL) {
		IterAddress = locals;
		while (IterAddress) {

			Unicast = IterAddress->FirstUnicastAddress;
			if (Unicast != NULL && IterAddress->OperStatus == IfOperStatusUp && IterAddress->IfType != IF_TYPE_SOFTWARE_LOOPBACK) {
				for (i = 0; Unicast != NULL; i++)
				{
					sockaddr_in* addr = (sockaddr_in*)Unicast->Address.lpSockaddr;
					LocalIP = htonl(addr->sin_addr.S_un.S_addr);
					//inet_ntop(AF_INET, (in_addr*)(&addr->sin_addr), LocalIP.data(), 16);
					//Logger::log("Local IP Found : {}", LocalIP.data());

					Unicast = Unicast->Next;
				}
				//IterAddress->TransmitLinkSpeed; for later use
				//IterAddress->ReceiveLinkSpeed;
			}

			IterAddress = IterAddress->Next;
		}
	}
	else {
		Logger::log("Call to GetAdaptersAddresses failed with error: {}\n", WSResult);
		if (WSResult == ERROR_NO_DATA)
			Logger::log("\tNo addresses were found for the requested parameters\n");
	}

	if (locals) {
		FREE(locals);
	}

	return LocalIP;
}


session::session(sessions& sessions, PCSTR IP, unsigned short port, int MTU_Size, WinForge* Link_) 
{
	Sessions = sessions;
	wsaData = sessions.wsaData;
	IOCP = Sessions.IOCP;

	SPoolHead = 0;
	RPoolHead = 0;

	MTU = MTU_Size;
	PreSetBufferMTU();
	address = Sessions.CreateAddress(IP, port);
	socketR = Sessions.CreateSocket();
	RegIOCP(socketR);
	InitReceiver(62485);
	
	Link = Link_;
}




int session::_requestHandshake() {
	return 0;
}
int session::_verifyHandshake() {
	return 0;
}
int session::_establishLink() {
	return 0;
}


std::string _GetLocalAddr() {
	return "192.168.1.59";
}

void session::RegIOCP(SOCKET& socket) {
	if (IOCP == NULL) {
		ULONG_PTR CompletionKey = 0;
		IOCP = CreateIoCompletionPort((HANDLE*)socket, NULL, CompletionKey, 1);
		if (IOCP == NULL) {
			OutputDebugStringA("IOCP Port Creation Failed Successfully\n");
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
					OutputDebugStringA((std::to_string(GetLastError()) + "type \n").c_str());
					OutputDebugStringA("Completion Status Get False\n");
					if (OVStruct != nullptr) {
						OutputDebugStringA("Completion Status Get OVStruct Failed\n");
					}
					else {
						OutputDebugStringA("IOCP Thread Going Down...\n");
					}
				}

				RECV_BUF* Buffer = reinterpret_cast<RECV_BUF*>(OVStruct);

				switch (Buffer->Type) {
				case OP_RECV:
					switch (*(Buffer->TransmitBuffer.buf + BufferSize - 1)) {
					case FrameStart:
						//start = Clock::now();
						RecvChunkStart = RPoolHead;
						RecvChunkLen += BufferSize;
						RPoolHead = (RPoolHead + 1) & 255;
						break;
					case FrameData:
						RecvChunkLen += BufferSize;
						RPoolHead = (RPoolHead + 1) & 255;
						break;
					case FrameEnd:
						RecvChunkEnd = RPoolHead;
						
						RecvChunkLen = RecvChunkEnd * MTU;
						RecvChunkLen += BufferSize-3;
						if (RecvChunkLen > 300000) {
							OutputDebugString(L"ffffffffffffffffffffffffffff\n");
							RecvChunkLen = 0;
							RPoolHead = 0;
							break;
						}
						Link->SetBufferData(&RecvBufferPool[0], RecvChunkLen);
						Link->SetRenderEvent();


						RecvChunkLen = 0;
						RPoolHead = 0;
						//OutputDebugStringA((std::to_string(std::chrono::duration_cast<Mlliseconds>((Clock::now() - start)).count()) + "\n").c_str());


						break;
					}

					PostWSARecv();
					break;
				}

				

			}
		}
	);

	StatusQueue.detach();

}

void session::CreateSesssionIOCP(PCSTR IP, unsigned short port) {

	socketR = INVALID_SOCKET;

	socketR = WSASocket(AF_INET, SOCK_DGRAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
	if (socketR == INVALID_SOCKET) {
		WSACleanup();
		OutputDebugStringA("Socket Did Not Come Home !");
		return;
	}

	RegIOCP(socketR);

	address.sin_family = AF_INET;
	address.sin_port = htons(port);
	inet_pton(AF_INET, IP, &address.sin_addr);

	WSResult = connect(socketR, (sockaddr*)&address, sizeof(address));
	if (WSResult != 0) {
		OutputDebugStringA(("Connection Failed : " + std::to_string(WSResult) + "\n").c_str());
	}
	else {
		OutputDebugStringA("Connected !\n");
	}



}


void  session::InitReceiver(unsigned int port) {

	sockaddr_in local;
	local.sin_family = AF_INET;
	local.sin_port = htons(port);
	inet_pton(AF_INET, "192.168.1.59", &local.sin_addr);

	WSResult = bind(socketR, (sockaddr*) &local, sizeof(local));
	if (WSResult != 0) {
		WSResult = WSAGetLastError();
		OutputDebugStringA((std::to_string(WSResult) + "\n").c_str());
		OutputDebugStringA("Bind Failed Successfully\n");
	}

	PostWSARecv();
	if (WSAGetLastError() != WSA_IO_PENDING) {
		OutputDebugStringA((std::to_string(WSResult)+"Recv Pre Post Failed\n").c_str());
	}
}



void session::ChunkedSend(CHAR* data, int data_size) {
	int MTU_slices = data_size - (data_size % MTU);
	CHeaderPool[SPoolHead].PacketType = Frame;
	CHeaderPool[SPoolHead].Target = 0;
	CHeaderPool[SPoolHead].ChunkIndex = FrameStart;
	TransmitPool[SPoolHead].TransmitBuffer[1].buf = reinterpret_cast<CHAR*>(&CHeaderPool[SPoolHead]);
	TransmitPool[SPoolHead].TransmitBuffer[0].buf = data;


	WSASendTo(
		socketR,
		TransmitPool[SPoolHead].TransmitBuffer,
		2,
		NULL, 0,
		(sockaddr*)&address,
		sizeof(address),
		&TransmitPool[SPoolHead].OVStruct,
		NULL
	);


	SPoolHead = (SPoolHead + 1) & 255;

	for (int offset = MTU; offset < MTU_slices-MTU; offset += MTU) {
		CHeaderPool[SPoolHead].PacketType = Frame;
		CHeaderPool[SPoolHead].Target = 0;
		CHeaderPool[SPoolHead].ChunkIndex = FrameData;
		TransmitPool[SPoolHead].TransmitBuffer[1].buf = reinterpret_cast<CHAR*>(&CHeaderPool[SPoolHead]);
		TransmitPool[SPoolHead].TransmitBuffer[0].buf = data + offset;



		WSASendTo(
			socketR,
			TransmitPool[SPoolHead].TransmitBuffer,
			2,
			NULL, 0,
			(sockaddr*)&address,
			sizeof(address),
			&TransmitPool[SPoolHead].OVStruct,
			NULL
		);


		SPoolHead = (SPoolHead + 1) & 255;

	}

	

	CHeaderPool[SPoolHead].PacketType = Frame;
	CHeaderPool[SPoolHead].Target = 0;
	if (MTU_slices != data_size) {
		CHeaderPool[SPoolHead].ChunkIndex = FrameData;
	}
	else {
		CHeaderPool[SPoolHead].ChunkIndex = FrameEnd;
	}
	TransmitPool[SPoolHead].TransmitBuffer[1].buf = reinterpret_cast<CHAR*>(&CHeaderPool[SPoolHead]);
	TransmitPool[SPoolHead].TransmitBuffer[0].buf = data + MTU_slices - MTU;



	WSASendTo(
		socketR,
		TransmitPool[SPoolHead].TransmitBuffer,
		2,
		NULL, 0,
		(sockaddr*)&address,
		sizeof(address),
		&TransmitPool[SPoolHead].OVStruct,
		NULL
	);


	SPoolHead = (SPoolHead + 1) & 255;

	if (MTU_slices != data_size) {
		CHeaderPool[SPoolHead].PacketType = Frame;
		CHeaderPool[SPoolHead].Target = 0;
		CHeaderPool[SPoolHead].ChunkIndex = FrameEnd;
		TransmitPool[SPoolHead].TransmitBuffer[1].buf = reinterpret_cast<CHAR*>(&CHeaderPool[SPoolHead]);
		TransmitPool[SPoolHead].TransmitBuffer[0].buf = data + MTU_slices;
		TransmitPool[SPoolHead].TransmitBuffer[0].len = data_size % MTU;


		WSASendTo(
			socketR,
			TransmitPool[SPoolHead].TransmitBuffer,
			2, NULL, 0,
			(sockaddr*)&address, sizeof(address),
			&TransmitPool[SPoolHead].OVStruct, NULL);

		TransmitPool[SPoolHead].TransmitBuffer[0].len = MTU;

		SPoolHead = (SPoolHead + 1) & 255;
	}

	


}

