#pragma once

#include "Mach1WebSocketServer.h"

#include <Poco/Net/HTTPRequestHandler.h>
#include <Poco/Net/HTTPRequestHandlerFactory.h>
#include <Poco/Net/HTTPServerRequest.h>
#include <Poco/Net/HTTPServerResponse.h>

#include <Poco/Net/WebSocket.h>
#include <Poco/Net/NetException.h>

class WebSocketRequestHandler : public Poco::Net::HTTPRequestHandler
{
	std::function<void(char*, int)> callback;
	bool* needToStop;

public:

	WebSocketRequestHandler() {
		callback = nullptr;
		needToStop = nullptr;
	}

	void addHandler(std::function<void(char*, int)> callback)
	{
		this->callback = callback;
	}

	void setNeedToStop(bool* needToStop)
	{
		this->needToStop = needToStop;
	}

	void handleRequest(Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response)
	{
		try
		{
			Poco::Net::WebSocket ws(request, response);

			std::cout << "WebSocket connection established" << std::endl;

			int flags = 0;
			int recvDataSize = 0;

			Poco::Timespan timeout(200, 0);
			do
			{
				if (ws.poll(timeout, Poco::Net::Socket::SELECT_READ))
				{
					char buffer[1024] = { 0, };
					recvDataSize = ws.receiveFrame(buffer, sizeof(buffer), flags);
					if (recvDataSize > 0 && (flags & Poco::Net::WebSocket::FRAME_OP_BITMASK) != Poco::Net::WebSocket::FRAME_OP_CLOSE)
					{
						if (callback) {
							callback(buffer, recvDataSize);
						}
						//ws.sendFrame(buffer, recvDataSize, flags);
					}
				}
			} while ((needToStop && *needToStop == false) && recvDataSize > 0 && (flags & Poco::Net::WebSocket::FRAME_OP_BITMASK) != Poco::Net::WebSocket::FRAME_OP_CLOSE);

			std::cout << "WebSocket connection closed." << std::endl;
		}
		catch (Poco::Net::WebSocketException& exc)
		{
			std::cout << "WebSocket error" << std::endl;
		}
		catch (Poco::Exception& e)
		{
			std::cout << "WebSocket error" << std::endl;
		}
	}
};


class DummyRequestHandler : public Poco::Net::HTTPRequestHandler
{
public:
	void handleRequest(Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response)
	{
		std::cout << "DummyRequestHandler: " << request.getURI() << std::endl;

		response.setStatusAndReason(Poco::Net::HTTPResponse::HTTP_OK);
		const char * data = "Ok.\n";
		response.sendBuffer(data, strlen(data));
	}
};

class SimpleRequestHandlerFactory : public Poco::Net::HTTPRequestHandlerFactory
{
	std::function<void(char*, int)> callback;
	bool* needToStop;

public:
	void addHandler(std::function<void(char*, int)> callback)
	{
		this->callback = callback;
	}

	void setNeedToStop(bool* needToStop)
	{
		this->needToStop = needToStop;
	}

	Poco::Net::HTTPRequestHandler* createRequestHandler(const Poco::Net::HTTPServerRequest& request)
	{
		std::cout << "SimpleRequestHandlerFactory: " << request.getURI() << std::endl;

		if (request.find("Upgrade") != request.end() && Poco::icompare(request["Upgrade"], "websocket") == 0) {
			WebSocketRequestHandler* webSocketRequestHandler = new WebSocketRequestHandler();
			webSocketRequestHandler->addHandler(callback);
			webSocketRequestHandler->setNeedToStop(needToStop);
			return webSocketRequestHandler;
		}
		return new DummyRequestHandler();
	}
};


void Mach1WebSocketServer::threadFunc() {

	Poco::Net::ServerSocket serverSocket(port);

	Poco::Net::HTTPServerParams* pParams = new Poco::Net::HTTPServerParams;
	Poco::Timespan timeout(20, 0);
	pParams->setTimeout(timeout);

	SimpleRequestHandlerFactory* simpleRequestHandlerFactory = new SimpleRequestHandlerFactory();
	simpleRequestHandlerFactory->addHandler(callback);
	simpleRequestHandlerFactory->setNeedToStop(&needToStop);

	httpServer = new Poco::Net::HTTPServer(simpleRequestHandlerFactory, serverSocket, pParams);
	httpServer->start();

	while (!needToStop) {
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}

	httpServer->stop();
}

Mach1WebSocketServer::Mach1WebSocketServer() {
	isRunning = false;
	needToStop = false;
	port = 9000;

	httpServer = nullptr;
}

Mach1WebSocketServer::~Mach1WebSocketServer() {
	stop();
}

void Mach1WebSocketServer::setPort(int port) {
	this->port = port;
}

void Mach1WebSocketServer::start() {
	stop();
	isRunning = true;
	needToStop = false;
	thread = std::thread(&Mach1WebSocketServer::threadFunc, this);
	thread.detach();
}

void Mach1WebSocketServer::stop() {
	if (isRunning) {
		needToStop = true;
		isRunning = false;
		std::this_thread::sleep_for(std::chrono::milliseconds(200));
	}
}

void Mach1WebSocketServer::addHandler(std::function<void(char*, int)> callback)
{
	this->callback = callback;
}
