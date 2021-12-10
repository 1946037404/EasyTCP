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
			printf("�˳��߳�\n");
			return;
		}
		else printf("��֧�ִ�����\n");
	}
}

int main()
{
	
	EasyTcpServer server;
	server.InitSocket();
	server.Bind(nullptr, 4567);
	server.Listen(5);
	server.Start(4);
	//����UI�߳�
	thread t(cmdThread);
	t.detach();
	while (g_bRun)
	{
		server.OnRun();
		//printf("����ʱ�䴦���������ݡ�����\n");
	} 
	server.Close();
	printf("���˳�\n");
	getchar();
	return 0;
}