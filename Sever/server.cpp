#include "EasyTcpServer.hpp"

using namespace std;
 
//#pragma comment(lib,"ws2_32,lib")
#include <thread>

bool g_bRun = true;
void cmdThread()
{
	while (true)
	{
		char cmdbuf[256] = {};
		scanf("%s", cmdbuf);
		if (strcmp(cmdbuf, "exit") == 0)
		{
			g_bRun = false;
			printf("退出线程\n");
			return;
		}
		else printf("不支持此命令\n");
	}
}

int main()
{
	
	EasyTcpServer server;
	server.InitSocket();
	server.Bind(nullptr, 4567);
	server.Listen(5);
	server.Start(4);
	//启动UI线程
	thread t(cmdThread);
	t.detach();
	while (g_bRun)
	{
		server.OnRun();
		//printf("空闲时间处理其他数据。。。\n");
	} 
	server.Close();
	printf("已退出\n");
	getchar();
	return 0;
}