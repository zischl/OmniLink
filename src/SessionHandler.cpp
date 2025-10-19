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


void sessions::GetLocals(uint8_t family, std::vector<sockaddr_in>* Buffer)
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

	do {

		locals = (IP_ADAPTER_ADDRESSES*)MEMALLOC(locals_size);
		if (locals == NULL)
		{
			return;
		}

		WSResult = GetAdaptersAddresses(Family, Flags, NULL, locals, &locals_size);
		if (WSResult != ERROR_BUFFER_OVERFLOW && WSResult != ERROR_SUCCESS)
		{
			Logger::log("Error {}: Could Not Retrieve Local Addresses !\n", WSResult);
			FREE(locals);
		}

		Retries--;

	} while (WSResult != ERROR_SUCCESS && Retries != 0);

	if (WSResult == ERROR_SUCCESS && locals != NULL)
	{
		IterAddress = locals;
		while (IterAddress)
		{

			Unicast = IterAddress->FirstUnicastAddress;

			if (Unicast != NULL &&
				IterAddress->OperStatus == IfOperStatusUp &&
				IterAddress->IfType != IF_TYPE_SOFTWARE_LOOPBACK
				&& IterAddress->IfType != IF_TYPE_TUNNEL
				&& (
					(IterAddress->PhysicalAddress[0] != 0x00 && IterAddress->PhysicalAddress[1] != 0x00) ||
					(IterAddress->PhysicalAddress[0] != 0x00 && IterAddress->PhysicalAddress[1] != 0xFF)
					)
				)
			{
				for (i = 0; Unicast != NULL; i++)
				{
					sockaddr_in* addr = (sockaddr_in*)Unicast->Address.lpSockaddr;
					Buffer->push_back(*addr);

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

	return;
}


void sessions::RegIOCP(HANDLE& IOCP, SOCKET& socket, const ULONG_PTR CompletionKey) {
	if (IOCP == NULL) {
		IOCP = CreateIoCompletionPort((HANDLE*)socket, NULL, CompletionKey, 0);
		if (IOCP == NULL) {
			OutputDebugStringA("IOCP Port Creation Failed Successfully\n");
		}

		return;
	}
	else {

	}
	IOCP = CreateIoCompletionPort((HANDLE*)socket, NULL, CompletionKey, 0);
	if (IOCP == NULL) {
		OutputDebugStringA("IOCP Port Binding Failed Successfully\n");
	}

}


void  sessions::BindReceiver(PCSTR IP, unsigned int port, SOCKET& socket) {
	int WSResult = 0;

	sockaddr_in local;
	local.sin_family = AF_INET;
	local.sin_port = htons(port);
	inet_pton(AF_INET, IP, &local.sin_addr);

	WSResult = bind(socket, (sockaddr*)&local, sizeof(local));
	if (WSResult != 0) {
		WSResult = WSAGetLastError();
		OutputDebugStringA((std::to_string(WSResult) + "\n").c_str());
		OutputDebugStringA("Bind Failed Successfully\n");
	}

}


void sessions::ConnectSesssion(const sockaddr_in& address, const SOCKET& socketR) {

	int WSResult = 0;

	WSResult = connect(socketR, (sockaddr*)&address, sizeof(address));
	if (WSResult != 0) {
		OutputDebugStringA(("Connection Failed : " + std::to_string(WSResult) + "\n").c_str());
	}

}


session::session(HANDLE& IOCP, PCSTR Local_IP, PCSTR IP, unsigned short port, int MTU_Size, OmniActiveInstance& SessionInstance) :
	Link(SessionInstance), MTU(MTU_Size)
{

	PreSetBufferMTU();
	address = sessions::CreateAddress(IP, port);
	socketR = sessions::CreateSocket();
	sessions::ConnectSesssion(address, socketR);
	sessions::RegIOCP(IOCP, socketR);
	sessions::StartCompletionPortHandlerThread(IOCP, socketR, RecvPool, SessionInstance);
	sessions::BindReceiver(Local_IP, 62485, socketR);

	sessions::PostWSARecv(socketR, RecvPool);
	if (WSAGetLastError() != WSA_IO_PENDING) {
		OutputDebugStringA("Recv Pre Post Failed\n");
	}

}





