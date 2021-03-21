#pragma once

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/streambuf.hpp>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <utility>
#include <boost/asio/ts/buffer.hpp>
#include <boost/asio/ts/internet.hpp>

#include "player.hpp"

#define wait_timeout 60

class PlayerSession : public std::enable_shared_from_this<PlayerSession>, public Player
{
public:
	PlayerSession(boost::asio::ip::tcp::socket activeSocket);
	void start();
	virtual void waitForAMatch() override;
	virtual void cancelWaiting() override;
	virtual void sendMessage(const std::string& message) override;
	virtual boost::asio::deadline_timer::duration_type deadline() const override;
	virtual void logout() override;
	virtual ~PlayerSession();
private:
	void read();
	void write();
	void closeSocket();

	boost::asio::ip::tcp::socket _active_socket;
	boost::asio::deadline_timer _timer;
	boost::asio::streambuf _request;
	boost::asio::streambuf _response;
	bool _close_socket;
};

class Server
{
public:
	static Server& instance()
	{
		static Server instance(7777);
		return instance;
	}
	void run();
	boost::asio::io_context& ioContext() {return _io_context;}
private:
	Server(short int port);
	//Handler when OS create an active socket when the passive socket receive a request
	void accept();
	
	boost::asio::io_context        _io_context;
	boost::asio::ip::tcp::acceptor _acceptor;
	boost::asio::ip::tcp::socket   _socket;
};
