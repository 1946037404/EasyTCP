#ifndef _EasyTcpServer_hpp_
#define _EasyTcpServer_hpp_

#include <iostream>
#include <stdio.h>
#include <vector>
#include <cstring>
#include <algorithm>
#include <thread>
#include <mutex>
#include <atomic>

#ifdef _WIN32
	#define FD_SETSIZE		2510
	#define WIN32_LEAN_AND_MEAN
	#define _WINSOCK_DEPRECATED_NO_WARNINGS
	#include <Windows.h>
	#include <WinSock2.h>
	#pragma comment(lib,"ws2_32.lib")
#else
	#include <unistd.h>
	#include <arpa/inet.h>
	#define SOCKET int
	#define INVALID_SOCKET	(SOCKET)	(~0)
	#define SOCKET_ERROR				(-1)
#endif

#include "MessageHeader.hpp"
#include "CELLTimestamp.hpp"

#ifndef RECV_BUFF_SIZE
#define RECV_BUFF_SIZE 10240
#endif


//�ͻ�����������
class ClientSocket
{
public:
	ClientSocket(SOCKET sockfd = INVALID_SOCKET)
	{
		_sockfd = sockfd;
		memset(_szMsgBuf, 0, sizeof(_szMsgBuf));
		_lastPos = 0;
	}

	SOCKET sockfd()
	{
		return _sockfd;
	}

	char * msgBuf()
	{
		return _szMsgBuf;
	}
	int getLast()
	{
		return _lastPos;
	}
	void setLastPos(int pos)
	{
		_lastPos = pos;
	}

	//����ָ��Socket����
	int SendData(DataHeader* header)
	{
		if (header != NULL)
			return send(_sockfd, (const char *)header, header->dataLength, 0);
		return SOCKET_ERROR;
	}
private:

	SOCKET _sockfd; //socket fd_set file desc set
	//�ڶ������� ��Ϣ������
	char _szMsgBuf[RECV_BUFF_SIZE * 5] = {};
	//��Ϣ������β��λ��
	int _lastPos;
};

//�����¼��ӿ�
class INetEvent
{
private:

public:
	//�ͻ����뿪�¼�
	virtual void OnNetLeave(ClientSocket * pClient) = 0;
	//�ͻ�����Ϣ�¼�
	virtual void OnNetMsg(ClientSocket * pClient,DataHeader* header) = 0;
	//�ͻ��˼����¼�
	virtual void OnNetJoin(ClientSocket * pClient) = 0;
};

class CellServer
{
private:
	SOCKET _sock;
	//��ʽ�ͻ�����
	std::vector< ClientSocket* > _clients;
	//�ͻ��������
	std::vector< ClientSocket* > _clientsBuff;
	//������е���
	std::mutex _mutex;
	std::thread _pThread;
	//�����¼�����
	INetEvent* _pNetEvent;
public:
	CellServer(SOCKET sock = INVALID_SOCKET)
	{
		_sock = sock;
		_pNetEvent = nullptr;
	}
	virtual ~CellServer()
	{
		Close();
		_sock = INVALID_SOCKET;
		//_clients.clear();
		//_recvCount = 0;
		//_clientsBuff.clear();
	}
	
	void setEventObj(INetEvent* event)
	{
		_pNetEvent = event;
	}

	bool OnRun()
	{
		while (isRun())
		{
			//�ӻ��������ȡ���ͻ�����
			if (_clientsBuff.size() > 0)
			{
				std::lock_guard<std::mutex> lock(_mutex);
				for (auto pClient : _clientsBuff)
				{
					_clients.push_back(pClient);
				}
				_clientsBuff.clear();
			}
			//���û����Ҫ����Ŀͻ��ˣ�������
			if (_clients.empty())
			{
				std::chrono::milliseconds t(1);
				std::this_thread::sleep_for(t);
				continue;
			}
			//������ socket
			fd_set fdRead;
			//������
			FD_ZERO(&fdRead);
			//��������socket���뼯��
			SOCKET maxSock = _clients[0]->sockfd();
			for (int n = (int)_clients.size() - 1;n >= 0;--n)
			{
				FD_SET(_clients[n]->sockfd(), &fdRead);
				if (maxSock < _clients[n]->sockfd())
				{
					maxSock = _clients[n]->sockfd();
				}
			}
			//nfds��һ������ ��ָfd_set������������������socket���ķ�Χ������������
			//���������ļ����������ֵ+1����windows�������������д0
			timeval t = { 1,0 };
			int ret = select(maxSock + 1, &fdRead, 0, 0, 0);
			if (ret < 0)
			{
				printf("select����������ͻ������˳�\n");
				Close();
				return false;
			}
			for (int n = (int)_clients.size() - 1;n >= 0;--n)
			{
				if (FD_ISSET(_clients[n]->sockfd(), &fdRead))
				{
					if (RecvData(_clients[n]) == -1)
					{
						auto iter = _clients.begin() + n;
						if (iter != _clients.end())
						{
							if(_pNetEvent)
							_pNetEvent->OnNetLeave(_clients[n]);
							delete _clients[n];
							iter = _clients.erase(iter);
						}
					}
				}
			}
		}
	}
	//�Ƿ�����
	bool isRun()
	{
		return _sock != INVALID_SOCKET;
	}
	//�ر�socket
	void Close()
	{
		if (_sock != INVALID_SOCKET)
		{
			//8.�ر��׽���
#ifdef _WIN32
			for (int n = _clients.size()-1 ; n >=0 ; n--)
			{
				closesocket(_clients[n]->sockfd());
				delete _clients[n];
			}
			closesocket(_sock);
#else
			for (size_t n = 0;n < (int)_clients.size();n++)
			{
				close(_clients[n]->sockfd());
				delete _clients[n];
			}
			close(_sock);
#endif
			_clients.clear();
		}
	}
	//�û���������
	char _szRecv[RECV_BUFF_SIZE] = {};
	//�������� ����ճ�� �ְ�
	int RecvData(ClientSocket* pClient)
	{
		// 5.���տͻ�������
		int nlen = (int)recv(pClient->sockfd(), _szRecv, RECV_BUFF_SIZE, 0);
		if (nlen <= 0)
		{
			//printf("�ͻ���<Socket = %d> ���˳�\n", pClient->sockfd());
			return -1;
		}
		//����ȡ�������ݿ�������Ϣ������
		memcpy(pClient->msgBuf() + pClient->getLast(), _szRecv, nlen);
		//��Ϣ������������β��λ�ú���
		pClient->setLastPos(pClient->getLast() + nlen);
		//�ж���Ϣ�����������ݳ��ȴ�����ϢͷDataheader�ĳ���
		while (pClient->getLast() >= sizeof(DataHeader))
		{
			//��ʱ���ǾͿ���֪����ǰ��Ϣ��ĳ���
			DataHeader* header = (DataHeader *)pClient->msgBuf();

			if (pClient->getLast() >= header->dataLength)
			{
				//ʣ��δ������Ϣ���������ݵĳ���
				const int nSize = pClient->getLast() - header->dataLength;
				//����������Ϣ�����ܻ�ı�header��
				OnNetMsg(pClient, header);
				//����Ϣ������ʣ��δ��������ǰ��
				memcpy(pClient->msgBuf(), pClient->msgBuf() + header->dataLength, nSize);
				//��Ϣ������������β��λ��ǰ��
				pClient->setLastPos(nSize);
			}
			//��Ϣ������ʣ�����ݲ���һ����������Ϣ
			else break;
		}
		return 0;
	}
	//��Ӧ������Ϣ
	virtual void OnNetMsg(ClientSocket* pClient, DataHeader* header)
	{
		
		_pNetEvent->OnNetMsg(pClient, header);
		/*
		auto t1 = _tTime.getElapsedSecond();
		if (t1 >= 1.0)
		{
			printf("time<%lf>,socket<%d>,clients<%d>,_recvCount<%d>\n", t1, _sock, _clients.size(), _recvCount);
			_recvCount = 0;
			_tTime.update();
		}
		*/
		switch (header->cmd)
		{
		case CMD_LOGIN:
		{
			//printf("1\n");
			Login* login = (Login *)header;
			//printf("�յ��ͻ���<Socket = %d>���� CMD_LOGIN , ���ݳ��ȣ� %d , userName = %s PassWord = %s\n", cSock, login->dataLength, login->userName, login->PassWord);
			//�����ж��û������Ƿ���ȷ�Ĺ��̣�
			//LoginResult ret;
			//pClient->SendData(&ret);
		}
		break;
		case CMD_LOGOUT:
		{
			Logout* logout = (Logout *)header;
			//	printf("�յ��ͻ���<Socket = %d>���� CMD_LOGOUT , ���ݳ��ȣ� %d , userName = %s \n", cSock, logout->dataLength, logout->userName);
			//�����ж��û������Ƿ���ȷ�Ĺ��̣�
			//LogoutResult ret;
			//SendData(cSock, &ret);
		}
		break;
		default:
		{
			printf("<Socket = %d>�յ�δ������Ϣ�����ݳ��ȣ� %d\n", pClient->sockfd(), header->dataLength);
			//DataHeader ret;
			//SendData(cSock, &ret);
		}
		break;
		}
	}

	void addClient(ClientSocket* pClient)
	{
		std::lock_guard<std::mutex> lock(_mutex);
		//_mutex.lock();
		_clientsBuff.push_back(pClient);
		//_mutex.unlock();
	}

	void Start()
	{
		_pThread = std::thread(std::mem_fn(&CellServer::OnRun), this);
	}

	size_t getClientCount()
	{
		return _clients.size() + _clientsBuff.size();
	}
};

class EasyTcpServer : public INetEvent
{
private:
	SOCKET _sock;
	//ÿ����Ϣ��ʱ
	CELLTimestamp _tTime;
	//��Ϣ��������ڲ��ᴴ���߳�
	std::vector<CellServer *> _cellServers;
	//�յ���Ϣ����
	std::atomic_int _recvCount;
	//�ͻ��˶Ͽ�����
	std::atomic_int _clientCount;
public:
	EasyTcpServer()
	{
		_sock = INVALID_SOCKET;
		_recvCount = 0;
		_clientCount = 0;
	}
	virtual ~EasyTcpServer()
	{
		//_clients.clear();
		//_cellServers.clear();
		Close();
	}
	//��ʼ��socket
	SOCKET InitSocket()
	{
		//����in Sock 2.x����
#ifdef _WIN32
		WORD ver = MAKEWORD(2, 2);
		WSADATA dat;
		WSAStartup(ver, &dat);
#endif
		//1.����һ��socket
		if (INVALID_SOCKET != _sock)
		{
			printf("<socket = %d>�رվ�����...\n", (int)_sock);
			Close();
		}
		_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (INVALID_SOCKET == _sock)
		{
			printf("error,����socket<%d>ʧ��\n", (int)_sock);
		}
		else printf("����Socket<%d>�ɹ�\n", (int)_sock);
		return _sock;
	}
	//��IP�Ͷ˿ں�
	int Bind(const char *ip, unsigned short port)
	{
		if (INVALID_SOCKET == _sock)
		{
			InitSocket();
		}
		sockaddr_in _sin = {};
		_sin.sin_family = AF_INET;
		_sin.sin_port = htons(port); // host to net unsigned short
#ifdef _WIN32
		if (ip)
		{
			_sin.sin_addr.S_un.S_addr = inet_addr(ip); // == inet_addr("127.0.0.1");  �������ַ
		}
		else
			_sin.sin_addr.S_un.S_addr = INADDR_ANY; // == inet_addr("127.0.0.1");  �������ַ
#else   
		if (ip)
		{
			_sin.sin_addr.s_addr = inet_addr(ip);
		}
		else
			_sin.sin_addr.s_addr = INADDR_ANY;
#endif
		int ret = (int)::bind(_sock, (sockaddr *)& _sin, sizeof(_sin));
		if (ret == SOCKET_ERROR)
		{
			printf("ERROR,�󶨶˿ں�<%d>ʧ��\n", port);
		}
		else
		{
			printf("�󶨶˿ں�<%d>�ɹ�\n", port);
		}
		return ret;
	}
	//�����˿ں�
	int Listen(int n)
	{
		int ret = listen(_sock, n);
		if (SOCKET_ERROR == ret)			//�������˽��м���;
		{
			printf("Socket = <%d>���󣬼����˿�ʧ��\n", (int)_sock);
		}
		else
		{
			printf("Socket = <%d>��������˿ڳɹ�\n", (int)_sock);
		}
		return ret;
	}
	//���ܿͻ��˽���
	SOCKET Accept()
	{
		//4.accpt:�ȴ����ܿͻ�������
		sockaddr_in clientAddr = {};     //Զ�̿ͻ��˵�ַ
		int nAddrLen = sizeof(sockaddr_in);
		SOCKET cSock = INVALID_SOCKET;
#ifdef _WIN32
		cSock = accept(_sock, (sockaddr *)&clientAddr, &nAddrLen);
#else
		cSock = accept(_sock, (sockaddr *)&clientAddr, (socklen_t *)&nAddrLen);
#endif			

		if (INVALID_SOCKET == cSock)
		{
			printf("Socket = <%d>������Ч�ͻ���\n", (int)_sock);
		}
		//���͸������ͻ���
		else 
		{
			//���¿ͻ��˷�����ͻ��������ٵ�cellServer
			addClientToCellServer(new ClientSocket(cSock));
		}
		return cSock;
	}

	void addClientToCellServer(ClientSocket* pClient)
	{
		//��ѯ�ͻ��������ٵ�CellServer��Ϣ����
		if (!_cellServers.empty())
		{
			auto pMinServer = _cellServers[0];
			for (auto pCellServer : _cellServers)
			{
				if (pMinServer->getClientCount() > pCellServer->getClientCount())
				{
					pMinServer = pCellServer;
				}
			}
			pMinServer->addClient(pClient);
			OnNetJoin(pClient);
		}
	}

	void Start(int nCellServer)
	{
		for (int i = 0;i < nCellServer;i++)
		{
			auto ser = new CellServer(_sock);
			_cellServers.push_back(ser);
			//ע�������¼����ܶ���
			ser->setEventObj(this);
			//������Ϣ�����߳�
			ser->Start();
			// ser;
		}
	}
	//�ر�socket
	void Close()
	{
		if (_sock != INVALID_SOCKET)
		{
#ifdef _WIN32
			//8.�ر��׽���
			closesocket(_sock);
			//���Windows����
			WSACleanup();
#else
			close(_sock);
#endif
		}
	}
	//����������Ϣ
	bool OnRun()
	{
		if (isRun())
		{
			time2msg();
			//������ socket
			fd_set fdRead;
			//fd_set fdWrite;
			//fd_set fdExp;

			FD_ZERO(&fdRead);
			//FD_ZERO(&fdWrite);
			//FD_ZERO(&fdExp);

			//��������socket���뼯��
			FD_SET(_sock, &fdRead);
			//FD_SET(_sock, &fdWrite);
			//FD_SET(_sock, &fdExp);
			//nfds��һ������ ��ָfd_set������������������socket���ķ�Χ������������
			//���������ļ����������ֵ+1����windows�������������д0
			timeval t = { 0,10 };
			int ret = select(_sock + 1, &fdRead, 0, 0, &t);
			if (ret < 0)
			{
				printf("select����������ͻ������˳�\n");
				Close();
				return false;
			}
			if (FD_ISSET(_sock, &fdRead))
			{
				FD_CLR(_sock, &fdRead);
				Accept();
				return true;
			}
			return true;
		}
		return false;
	}
	//�Ƿ�����
	bool isRun()
	{
		return _sock != INVALID_SOCKET;
	}

	//���������ÿ���յ���������Ϣ
	void time2msg()
	{
		auto t1 = _tTime.getElapsedSecond();
		if (t1 >= 1.0)
		{
			
			printf("thread<%d>,time<%lf>,socket<%d>,clients<%d>,_recvCount<%d>\n", _cellServers.size(), t1, _sock, (int)_clientCount, (int)(_recvCount / t1));
			_recvCount = 0;
			_tTime.update();
		}
	}

	//ֻ�ᱻһ���̴߳���
	virtual void OnNetJoin(ClientSocket *pClient)
	{
		_clientCount++;
	}
	//���̴߳��� ����ȫ
	virtual void OnNetLeave(ClientSocket * pClient)
	{
		_clientCount--;
	}
	virtual void OnNetMsg(ClientSocket * pClient, DataHeader* header)
	{
		_recvCount++;
	}
};

#endif