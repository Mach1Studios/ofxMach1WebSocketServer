#pragma once

#include <Poco/Net/HTTPServer.h>

#include <functional>
#include <iostream>
#include <thread>
#include <vector>

class Mach1WebSocketServer {
	Poco::Net::HTTPServer* httpServer;

	std::thread thread;
	bool isRunning;
	bool needToStop;
	int port;

	void threadFunc();

	std::function<void(char*, int)> callback;

public:
	Mach1WebSocketServer();
	~Mach1WebSocketServer();

	void setPort(int port);
	void start();
	void stop();

	void addHandler(std::function<void(char*, int)> callback);
};



