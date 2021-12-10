#ifndef _EasyTcpClient_hpp_
#define _EasyTcpClient_hpp_

#ifdef _WIN32
	#define _WINSOCK_DEPRECATED_NO_WARNINGS 0
	#define _WIN32_LEAN_AND_MEAN
	#include <WinSock2.h>
	#include <windows.h>
	# pragma comment(lib,"ws2_32.lib")
#else
	#include <unistd.h>
	#include <arpa/inet.h>
	#include <string.h>

	#define SOCKET int
#define INVALID_SOCKET(SOCKET)(~0)
#define SOCKET_ERROR			(-1)

#endif //win32
#include <stdio.h>
#include "MessageHeader.hpp"
using namespace std;


class EasyTcpClient
{
		SOCKET _sock;
		bool _isConnect;
public:
	EasyTcpClient()
	{
		//初始化
		_sock = INVALID_SOCKET;
		_isConnect = false;
	}

	virtual ~EasyTcpClient()
	{
		Close();
	}
	//初始化socket
	void InitSocket()
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
			printf("<socket = %d>关闭旧链接...\n", _sock);
			Close();
		}
		_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (INVALID_SOCKET == _sock)
		{
			printf("error\n");
		}
		//else printf("建立成功\n");

	}

	//连接服务器
	int Connect(char *ip,unsigned short port)
	{
		if (INVALID_SOCKET == _sock)
		{
			InitSocket();
		}
		//2.连接服务器 connect
		sockaddr_in _sin = {};
		_sin.sin_family = AF_INET;
		_sin.sin_port = htons(port); //连接端口号
#ifdef _WIN32
		_sin.sin_addr.S_un.S_addr = inet_addr(ip); //连接服务器地址 127为本机测试地址
#else
		_sin.sin_addr.s_addr = inet_addr(ip);
#endif
		int ret = connect(_sock, (sockaddr*)&_sin, sizeof(sockaddr_in));
		if (SOCKET_ERROR == ret)
		{
			printf("error\n");
		}
		else
		{
			_isConnect = true;
		}
		//else printf("连接服务器成功\n");
		return ret;
	}
	
	//关闭socket
	void Close()
	{
		//关闭 in Sock 2.x环境
		//关闭套接字closesocket
		if (_sock != INVALID_SOCKET) 
		{
#ifdef _WIN32
			closesocket(_sock);
			//清楚windows socket环境
			WSACleanup();
#else
			close(_sock);
#endif
			_sock = INVALID_SOCKET;
		}
		_isConnect = false;
	}

	//处理网络消息
	bool OnRun()
	{
		if (isRun())
		{
			fd_set fdRead;
			FD_ZERO(&fdRead);
			FD_SET(_sock, &fdRead, NULL, NULL);
			timeval t = { 1,0 };
			int ret = select(_sock + 1, &fdRead, 0, 0, &t);
			if (ret<0)
			{
				printf("<socket = %d>select任务结束1\n", _sock);
				return false;
			}
			if (FD_ISSET(_sock, &fdRead))
			{
				FD_CLR(_sock, &fdRead);

				if (RecvData(_sock) == -1)
				{
					printf("<socket = %d>select任务结束2\n", _sock);
					return false;
				}
			}
			return true;
		}
		return false;
	}

	//是否工作中
	bool isRun()
	{
		return _sock != INVALID_SOCKET && _isConnect;
	}
	
	//缓冲区最小单元大小
#ifndef RECV_BUFF_SIZE
#define RECV_BUFF_SIZE 10240
#endif
	//用缓冲区接收
	char _szRecv[RECV_BUFF_SIZE] = {};
	//第二缓冲区 消息缓冲区
	char _szMsgBuf[RECV_BUFF_SIZE*5] = {};
	//消息缓冲区尾部位置
	int _lastPos = 0;
	//接收数据 处理粘包 拆分包
	int RecvData(SOCKET _cSock)
	{
		// 5.接收客户端数据
		int nlen = (int)recv(_cSock, _szRecv, RECV_BUFF_SIZE, 0);
		if (nlen <= 0)
		{
			printf("<Socket = %d>与服务器断开连接，任务结束\n", _cSock);
			return -1;
		}
		//将收取到的数据拷贝到消息缓冲区
		memcpy(_szMsgBuf + _lastPos, _szRecv, nlen);
		//消息缓冲区的数据尾部位置后移
		_lastPos += nlen;
		//判断消息缓冲区的数据长度大于消息头Dataheader的长度
		while (_lastPos >= sizeof(DataHeader))
		{
			//这时我们就可以知道当前消息体的长度
			DataHeader* header = (DataHeader *)_szMsgBuf;

			if (_lastPos >= header->dataLength)
			{
				//剩余未处理消息缓冲区数据的长度
				const int nSize = _lastPos - header->dataLength;
				//处理网络消息（可能会改变header）
				OnNetMsg(header);
				//将消息缓冲区剩余未处理数据前移
				memcpy(_szMsgBuf, _szMsgBuf+header->dataLength, nSize);
				_lastPos = nSize;
			}
			else break;
		}
		return 0;
	}

	//响应网络消息
	virtual void OnNetMsg(DataHeader* header)
	{
		switch (header->cmd)
		{
		case CMD_LOGIN_RESULT:
		{
			LoginResult* login = (LoginResult *)header;
			//printf("<Socket = %d>收到服务器消息： CMD_LOGIN_RESULT , 数据长度： %d\n", _sock, login->dataLength);
		}
		break;
		case CMD_LOGOUT_RESULT:
		{
			LogoutResult* logout = (LogoutResult *)header;
			//printf("<Socket = %d>收到服务器消息： CMD_LOGOUT_RESULT , 数据长度： %d\n", _sock, logout->dataLength);
		}
		break;
		case CMD_NEW_USER_JOIN:
		{
			NewUserJoin* userJoin = (NewUserJoin *)header;
			//printf("<Socket = %d>收到服务器消息： CMD_NEW_USER_JOIN , 数据长度： %d\n", _sock, userJoin->dataLength);
		}
		break;
		case CMD_ERROR:
		{
			//printf("<Socket = %d>收到服务器消息： CMD_ERROR , 数据长度： %d\n", _sock , header->dataLength);
		}
		break;
		default:
		{
			printf("<Socket = %d>收到未定义消息，数据长度： %d\n", _sock, header->dataLength);
		}
		}
	}

	//发送数据
	int SendData(DataHeader* header)
	{
		int ret = SOCKET_ERROR;
		if (isRun() && header != NULL) 
		{
			ret = send(_sock, (const char *)header, header->dataLength, 0);
			if (ret == SOCKET_ERROR)
			{
				Close();
			}
		}
		return ret;
	}
private:


};


#endif