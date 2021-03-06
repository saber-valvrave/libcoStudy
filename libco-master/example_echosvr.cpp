/*
* Tencent is pleased to support the open source community by making Libco available.

* Copyright (C) 2014 THL A29 Limited, a Tencent company. All rights reserved.
*
* Licensed under the Apache License, Version 2.0 (the "License"); 
* you may not use this file except in compliance with the License. 
* You may obtain a copy of the License at
*
*	http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, 
* software distributed under the License is distributed on an "AS IS" BASIS, 
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. 
* See the License for the specific language governing permissions and 
* limitations under the License.
*/



#include "co_routine.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/time.h>
#include <stack>

#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/un.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <sys/wait.h>

#ifdef __FreeBSD__
#include <cstring>
#include <sys/types.h>
#include <sys/wait.h>
#endif

using namespace std;
struct task_t
{
	stCoRoutine_t *co;
	int fd;
};

static stack<task_t*> g_readwrite;
static int g_listen_fd = -1;
static int SetNonBlock(int iSock)
{
    int iFlags;

    iFlags = fcntl(iSock, F_GETFL, 0);
    iFlags |= O_NONBLOCK;
    iFlags |= O_NDELAY;
    int ret = fcntl(iSock, F_SETFL, iFlags);
    return ret;
}

static void *readwrite_routine( void *arg )
{

	co_enable_hook_sys();

	task_t *co = (task_t*)arg;
	char buf[ 1024 * 16 ];
	for(;;)
	{
		if( -1 == co->fd )
		{
			g_readwrite.push( co );
			co_yield_ct();
			continue;
		}

		int fd = co->fd;
		co->fd = -1;

		for(;;)
		{
			struct pollfd pf = { 0 };
			pf.fd = fd;
			pf.events = (POLLIN|POLLERR|POLLHUP);
			co_poll( co_get_epoll_ct(),&pf,1,1000);

			int ret = read( fd,buf,sizeof(buf) );
			if( ret > 0 )
			{
				ret = write( fd,buf,ret );
			}
			if( ret <= 0 )
			{
				// accept_routine->SetNonBlock(fd) cause EAGAIN, we should continue
				if (errno == EAGAIN)
					continue;
				close( fd );
				break;
			}
		}

	}
	return 0;
}
int co_accept(int fd, struct sockaddr *addr, socklen_t *len );
static void *accept_routine( void * )
{
	co_enable_hook_sys();
	printf("accept_routine\n");
	fflush(stdout);
	for(;;)
	{
		//printf("pid %ld g_readwrite.size %ld\n",getpid(),g_readwrite.size());
		//若无协程任务，则1s超时继续检测
		if( g_readwrite.empty() )
		{
			printf("empty\n"); //sleep
			struct pollfd pf = { 0 };
			pf.fd = -1;
			poll( &pf,1,1000);

			continue;

		}
		struct sockaddr_in addr; //maybe sockaddr_un;
		memset( &addr,0,sizeof(addr) );
		socklen_t len = sizeof(addr);
		//接受客户端连接，并申请存储空间，设置读写超时，将指向空间指针记录到以fd为下标的静态数组中（hook模块）
		INFO("接受客户端连接...");
		int fd = co_accept(g_listen_fd, (struct sockaddr *)&addr, &len);
		if( fd < 0 )
		{
			INFO("接受客户端连接[失败]...");
			struct pollfd pf = { 0 };
			pf.fd = g_listen_fd;
			pf.events = (POLLIN|POLLERR|POLLHUP);
			//连接失败后，该函数会切回主协程
			co_poll( co_get_epoll_ct(),&pf,1,1000 );
			continue;
		}
		//连接成功，若无任务则关闭连接
		INFO("接受客户端连接[成功]...");
		if( g_readwrite.empty() )
		{
			close( fd );
			continue;
		}
		SetNonBlock( fd );
		task_t *co = g_readwrite.top();
		co->fd = fd;
		g_readwrite.pop();
		//切到一个读写协程
		co_resume( co->co );
	}
	return 0;
}

static void SetAddr(const char *pszIP,const unsigned short shPort,struct sockaddr_in &addr)
{
	bzero(&addr,sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(shPort);
	int nIP = 0;
	if( !pszIP || '\0' == *pszIP   
	    || 0 == strcmp(pszIP,"0") || 0 == strcmp(pszIP,"0.0.0.0") 
		|| 0 == strcmp(pszIP,"*") 
	  )
	{
		nIP = htonl(INADDR_ANY);
	}
	else
	{
		nIP = inet_addr(pszIP);
	}
	addr.sin_addr.s_addr = nIP;

}

static int CreateTcpSocket(const unsigned short shPort /* = 0 */,const char *pszIP /* = "*" */,bool bReuse /* = false */)
{
	int fd = socket(AF_INET,SOCK_STREAM, IPPROTO_TCP);
	if( fd >= 0 )
	{
		if(shPort != 0)
		{
			if(bReuse)
			{
				int nReuseAddr = 1;
				setsockopt(fd,SOL_SOCKET,SO_REUSEADDR,&nReuseAddr,sizeof(nReuseAddr));
			}
			struct sockaddr_in addr ;
			SetAddr(pszIP,shPort,addr);
			int ret = bind(fd,(struct sockaddr*)&addr,sizeof(addr));
			if( ret != 0)
			{
				close(fd);
				return -1;
			}
		}
	}
	return fd;
}


int main(int argc,char *argv[])
{
	if(argc<5){
		printf("Usage:\n"
               "example_echosvr [IP] [PORT] [TASK_COUNT] [PROCESS_COUNT]\n"
               "example_echosvr [IP] [PORT] [TASK_COUNT] [PROCESS_COUNT] -d   # daemonize mode\n");
		return -1;
	}
	const char *ip = argv[1];
	int port = atoi( argv[2] );
	int cnt = atoi( argv[3] );
	int proccnt = atoi( argv[4] );
	bool deamonize = argc >= 6 && strcmp(argv[5], "-d") == 0;

	//创建网络socket并监听
	g_listen_fd = CreateTcpSocket( port,ip,true );
	listen( g_listen_fd,1024 );
	if(g_listen_fd==-1){
		printf("Port %d is in use\n", port);
		return -1;
	}
	printf("listen %d %s:%d\n",g_listen_fd,ip,port);
	//将监听套接字设置为非阻塞
	SetNonBlock( g_listen_fd );
	//创建工作进程
	for(int k=0;k<proccnt;k++)
	{

		pid_t pid = fork();
		if( pid > 0 )
		{
			continue;
		}
		else if( pid < 0 )
		{
			break;
		}
		//创建工作协程，包含多个读写协程和一个等待连接协程
		for(int i=0;i<cnt;i++)
		{
			task_t * task = (task_t*)calloc( 1,sizeof(task_t) );
			task->fd = -1;

			co_create( &(task->co),NULL,readwrite_routine,task );
			co_resume( task->co );

		}
		stCoRoutine_t *accept_co = NULL;
		co_create( &accept_co,NULL,accept_routine,0 );
		co_resume( accept_co );

		co_eventloop( co_get_epoll_ct(),0,0 );

		exit(0);
	}
	//非后台运行，等待子线程结束
	//后台运行：父进程直接返回，子进程变为僵尸进程被1号进程接收。
	if(!deamonize) wait(NULL);
	return 0;
}

