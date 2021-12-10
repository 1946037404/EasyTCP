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


//客户端数据类型
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

	//发送指定Socket数据
	int SendData(DataHeader* header)
	{
		if (header != NULL)
			return send(_sockfd, (const char *)header, header->dataLength, 0);
		return SOCKET_ERROR;
	}
private:

	SOCKET _sockfd; //socket fd_set file desc set
	//第二缓冲区 消息缓冲区
	char _szMsgBuf[RECV_BUFF_SIZE * 5] = {};
	//消息缓冲区尾部位置
	int _lastPos;
};

//网络事件接口
class INetEvent
{
private:

public:
	//客户端离开事件
	virtual void OnNetLeave(ClientSocket * pClient) = 0;
	//客户端消息事件
	virtual void OnNetMsg(ClientSocket * pClient,DataHeader* header) = 0;
	//客户端加入事件
	virtual void OnNetJoin(ClientSocket * pClient) = 0;
};

class CellServer
{
private:
	SOCKET _sock;
	//正式客户队列
	std::vector< ClientSocket* > _clients;
	//客户缓冲队列
	std::vector< ClientSocket* > _clientsBuff;
	//缓冲队列的锁
	std::mutex _mutex;
	std::thread _pThread;
	//网络事件对象
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
			//从缓冲队列里取出客户数据
			if (_clientsBuff.size() > 0)
			{
				std::lock_guard<std::mutex> lock(_mutex);
				for (auto pClient : _clientsBuff)
				{
					_clients.push_back(pClient);
				}
				_clientsBuff.clear();
			}
			//如果没有需要处理的客户端，就跳过
			if (_clients.empty())
			{
				std::chrono::milliseconds t(1);
				std::this_thread::sleep_for(t);
				continue;
			}
			//伯克利 socket
			fd_set fdRead;
			//清理集合
			FD_ZERO(&fdRead);
			//将描述符socket加入集合
			SOCKET maxSock = _clients[0]->sockfd();
			for (int n = (int)_clients.size() - 1;n >= 0;--n)
			{
				FD_SET(_clients[n]->sockfd(), &fdRead);
				if (maxSock < _clients[n]->sockfd())
				{
					maxSock = _clients[n]->sockfd();
				}
			}
			//nfds是一个整数 是指fd_set集合中所有描述符（socket）的范围，而不是数量
			//即是所有文件描述符最大值+1，在windows中这个参数可以写0
			timeval t = { 1,0 };
			int ret = select(maxSock + 1, &fdRead, 0, 0, 0);
			if (ret < 0)
			{
				printf("select任务结束，客户端已退出\n");
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
	//是否工作中
	bool isRun()
	{
		return _sock != INVALID_SOCKET;
	}
	//关闭socket
	void Close()
	{
		if (_sock != INVALID_SOCKET)
		{
			//8.关闭套接字
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
	//用缓冲区接收
	char _szRecv[RECV_BUFF_SIZE] = {};
	//接收数据 处理粘包 分包
	int RecvData(ClientSocket* pClient)
	{
		// 5.接收客户端数据
		int nlen = (int)recv(pClient->sockfd(), _szRecv, RECV_BUFF_SIZE, 0);
		if (nlen <= 0)
		{
			//printf("客户端<Socket = %d> 已退出\n", pClient->sockfd());
			return -1;
		}
		//将收取到的数据拷贝到消息缓冲区
		memcpy(pClient->msgBuf() + pClient->getLast(), _szRecv, nlen);
		//消息缓冲区的数据尾部位置后移
		pClient->setLastPos(pClient->getLast() + nlen);
		//判断消息缓冲区的数据长度大于消息头Dataheader的长度
		while (pClient->getLast() >= sizeof(DataHeader))
		{
			//这时我们就可以知道当前消息体的长度
			DataHeader* header = (DataHeader *)pClient->msgBuf();

			if (pClient->getLast() >= header->dataLength)
			{
				//剩余未处理消息缓冲区数据的长度
				const int nSize = pClient->getLast() - header->dataLength;
				//处理网络消息（可能会改变header）
				OnNetMsg(pClient, header);
				//将消息缓冲区剩余未处理数据前移
				memcpy(pClient->msgBuf(), pClient->msgBuf() + header->dataLength, nSize);
				//消息缓冲区的数据尾部位置前移
				pClient->setLastPos(nSize);
			}
			//消息缓冲区剩余数据不够一条完整的消息
			else break;
		}
		return 0;
	}
	//响应网络消息
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
			//printf("收到客户端<Socket = %d>请求： CMD_LOGIN , 数据长度： %d , userName = %s PassWord = %s\n", cSock, login->dataLength, login->userName, login->PassWord);
			//忽略判断用户密码是否正确的过程：
			//LoginResult ret;
			//pClient->SendData(&ret);
		}
		break;
		case CMD_LOGOUT:
		{
			Logout* logout = (Logout *)header;
			//	printf("收到客户端<Socket = %d>请求： CMD_LOGOUT , 数据长度： %d , userName = %s \n", cSock, logout->dataLength, logout->userName);
			//忽略判断用户密码是否正确的过程：
			//LogoutResult ret;
			//SendData(cSock, &ret);
		}
		break;
		default:
		{
			printf("<Socket = %d>收到未定义消息，数据长度： %d\n", pClient->sockfd(), header->dataLength);
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
	//每秒消息计时
	CELLTimestamp _tTime;
	//消息处理对象，内部会创建线程
	std::vector<CellServer *> _cellServers;
	//收到消息计数
	std::atomic_int _recvCount;
	//客户端断开计数
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
	//初始化socket
	SOCKET InitSocket()
	{
		//启动in Sock 2.x环境
#ifdef _WIN32
		WORD ver = MAKEWORD(2, 2);
		WSADATA dat;
		WSAStartup(ver, &dat);
#endif
		//1.建立一个socket
		if (INVALID_SOCKET != _sock)
		{
			printf("<socket = %d>关闭旧链接...\n", (int)_sock);
			Close();
		}
		_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (INVALID_SOCKET == _sock)
		{
			printf("error,建立socket<%d>失败\n", (int)_sock);
		}
		else printf("建立Socket<%d>成功\n", (int)_sock);
		return _sock;
	}
	//绑定IP和端口号
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
			_sin.sin_addr.S_un.S_addr = inet_addr(ip); // == inet_addr("127.0.0.1");  绑定网络地址
		}
		else
			_sin.sin_addr.S_un.S_addr = INADDR_ANY; // == inet_addr("127.0.0.1");  绑定网络地址
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
			printf("ERROR,绑定端口号<%d>失败\n", port);
		}
		else
		{
			printf("绑定端口号<%d>成功\n", port);
		}
		return ret;
	}
	//监听端口号
	int Listen(int n)
	{
		int ret = listen(_sock, n);
		if (SOCKET_ERROR == ret)			//最大多少人进行监听;
		{
			printf("Socket = <%d>错误，监听端口失败\n", (int)_sock);
		}
		else
		{
			printf("Socket = <%d>监听网络端口成功\n", (int)_sock);
		}
		return ret;
	}
	//接受客户端接连
	SOCKET Accept()
	{
		//4.accpt:等待接受客户端连接
		sockaddr_in clientAddr = {};     //远程客户端地址
		int nAddrLen = sizeof(sockaddr_in);
		SOCKET cSock = INVALID_SOCKET;
#ifdef _WIN32
		cSock = accept(_sock, (sockaddr *)&clientAddr, &nAddrLen);
#else
		cSock = accept(_sock, (sockaddr *)&clientAddr, (socklen_t *)&nAddrLen);
#endif			

		if (INVALID_SOCKET == cSock)
		{
			printf("Socket = <%d>错误，无效客户端\n", (int)_sock);
		}
		//发送给其他客户端
		else 
		{
			//将新客户端分配给客户数量最少的cellServer
			addClientToCellServer(new ClientSocket(cSock));
		}
		return cSock;
	}

	void addClientToCellServer(ClientSocket* pClient)
	{
		//查询客户数量最少的CellServer消息处理
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
			//注册网络事件接受对象
			ser->setEventObj(this);
			//启动消息处理线程
			ser->Start();
			// ser;
		}
	}
	//关闭socket
	void Close()
	{
		if (_sock != INVALID_SOCKET)
		{
#ifdef _WIN32
			//8.关闭套接字
			closesocket(_sock);
			//清除Windows环境
			WSACleanup();
#else
			close(_sock);
#endif
		}
	}
	//处理网络消息
	bool OnRun()
	{
		if (isRun())
		{
			time2msg();
			//伯克利 socket
			fd_set fdRead;
			//fd_set fdWrite;
			//fd_set fdExp;

			FD_ZERO(&fdRead);
			//FD_ZERO(&fdWrite);
			//FD_ZERO(&fdExp);

			//将描述符socket加入集合
			FD_SET(_sock, &fdRead);
			//FD_SET(_sock, &fdWrite);
			//FD_SET(_sock, &fdExp);
			//nfds是一个整数 是指fd_set集合中所有描述符（socket）的范围，而不是数量
			//即是所有文件描述符最大值+1，在windows中这个参数可以写0
			timeval t = { 0,10 };
			int ret = select(_sock + 1, &fdRead, 0, 0, &t);
			if (ret < 0)
			{
				printf("select任务结束，客户端已退出\n");
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
	//是否工作中
	bool isRun()
	{
		return _sock != INVALID_SOCKET;
	}

	//计数并输出每秒收到的网络消息
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

	//只会被一个线程触发
	virtual void OnNetJoin(ClientSocket *pClient)
	{
		_clientCount++;
	}
	//多线程触发 不安全
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