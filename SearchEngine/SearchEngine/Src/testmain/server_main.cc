//与test_ser.cc不同在于: 一次性发送服务器内容

#include "../Lib/cppjieba/src/Jieba.hpp"
#include "../Inc/PreRipePage.h"
#include "../Inc/Index.h"
#include "../Inc/TcpConnection.h"
#include "../Inc/EpollPoller.h"
#include "../Inc/InetAddress.h"
#include "../Inc/SearchTask.h"
#include "../Inc/TcpServer.h"
#include "../Inc/Socket.h"
#include "../Inc/Conf.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <iostream>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdio.h>
#include <string>



void sendMsg(guoshubo::TcpConnection *pTcpConn, std::string &sendstr)
{
	std::string::size_type len = sendstr.size();
	pTcpConn->sendmsg((const char *)&len, (size_t)sizeof(int));
	pTcpConn->sendmsg(sendstr);
}


char *myMmap(guoshubo::Conf &conf, size_t &filelen)
{
	//0.打开文件，获取文件大小
	const char *filepath = conf.getPagePath().c_str();
	int fd = ::open(filepath, O_RDONLY);
	if(-1 == fd)
	{
		perror("open ripepage.lib error");
		exit(EXIT_FAILURE);
	}	

	struct stat statbuf;
	int ret = ::stat(filepath, &statbuf);
	if(-1 == ret)
	{
		perror("stat error");
		exit(EXIT_FAILURE);
	}
	filelen = statbuf.st_size;
	
	//1. 映射
	char *filestart;
	filestart = (char *)mmap(NULL, filelen, PROT_READ, MAP_PRIVATE, fd, 0);
	if(filestart == (char *)-1)
	{
		perror("mmap error");
		exit(EXIT_FAILURE);
	}
	
	::close(fd);
	return filestart;
}


void myConnection(guoshubo::TcpConnection *pTcpConn)
{
	std::cout << pTcpConn->toString() << "已连接" << std::endl;
	std::string connmsg = "欢迎使用宇宙无敌超级搜索引擎";	
	pTcpConn->sendmsg(connmsg);
}


void myMessage(guoshubo::TcpConnection *pTcpConn)
{
	std::string recvstr = pTcpConn->recvmsg();
	std::cout << "正在处理数据：" << recvstr <<"..." << std::endl;

	guoshubo::SearchTask mytask = pTcpConn->getSearchTask();
	mytask.setKeyword(recvstr);
	mytask.process();
	std::string sendstr = mytask.getResult();

	//处理待传输数据
	sendMsg(pTcpConn, sendstr);
	//std::cout << "处理完毕" << std::endl << std::endl;
}


void myClose(guoshubo::TcpConnection *pTcpConn)
{
	std::cout << pTcpConn->toString() << "已断开" << std::endl;
}


int main()
{
	//0. 分词/配置文件准备
	cppjieba::Jieba mycut("../Lib/cppjieba/dict/jieba.dict.utf8",
						  "../Lib/cppjieba/dict/hmm_model.utf8",
						  "../Lib/cppjieba/dict/user.dict.utf8");

	guoshubo::Conf myconf("/home/fiona/PROJECT@WANGDAO/c++/SearchEngine/Conf/server.conf");
	std::vector<guoshubo::Page> mypage;

	//1. 选择是否更新数据（离线）
	std::cout << "===========================" << std::endl;
	std::cout << "0.0 是否需要更新数据?(y/n)" << std::endl;

	char answer;
	int index = 3;
	bool flag = false;
	while(index --)
	{
		std::cin >> answer;
		if(answer != 'y' && answer != 'n')
			std::cout << "请选择y/n：" << std::endl;
		else
		{
			flag = true;
			break;
		}
	}
	if(!flag)
	{
		std::cout << "0.0 输入错误 将退出程序" << std::endl;
		::sleep(1);
		std::cout << "===========================" << std::endl;
		exit(EXIT_FAILURE);
	}	
	else
	{
		if(answer == 'y')
		{
			std::cout << "正在创建 网页库..." << std::endl;
			guoshubo::PreRipePage myripe(myconf, mypage, mycut);
			myripe.createLibs();

			std::cout << "正在创建 索引文件..." << std::endl;
			guoshubo::Index myindex(myconf, mypage);
			myindex.createIndex();
			std::cout << "0.0 更新完毕 感谢耐心的你" << std::endl;
			std::cout << "===========================" << std::endl;
		}
		else
		{
			std::cout << "===========================" << std::endl;
		}
	}

	//2. 服务器与客户端交互（在线）
	std::cout << "===========================" << std::endl;
	std::cout << "0.0 服务器准备中..." << std::endl;
 
	const char *pIp = myconf.getIp().c_str();
	unsigned short port = myconf.getPort();

	size_t filelen;
	char *pagestart = myMmap(myconf, filelen);	

	guoshubo::SearchTask::IndexMap indexMap = myconf.createIndexMap();
	guoshubo::SearchTask::OffsetMap offsetMap = myconf.createOffset();
	guoshubo::SearchTask mytask(pagestart, indexMap, offsetMap, mycut);

	guoshubo::TcpServer myser(pIp, port, 10, mytask);
	
	myser.setConnectionCb(myConnection);
	myser.setMessageCb(myMessage);
	myser.setCloseCb(myClose);
	
	std::cout << "0.0 服务器已启动" << std::endl;
	std::cout << "===========================" << std::endl;
	myser.start();
}
