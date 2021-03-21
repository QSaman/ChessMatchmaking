#include "server.hpp"
#include "manager.hpp"

#include <boost/asio/deadline_timer.hpp>
#include <boost/asio/read_until.hpp>
#include <boost/asio/write.hpp>
#include <boost/system/error_code.hpp>

#include <memory>

PlayerSession::PlayerSession(boost::asio::ip::tcp::socket activeSocket) :
	_active_socket(std::move(activeSocket)),
	_timer(boost::asio::deadline_timer(Server::instance().ioContext())),
	_close_socket(false)
{
}

void PlayerSession::logout()
{
	_close_socket = true;
}

void PlayerSession::closeSocket()
{
	if (!_active_socket.is_open())
	{
		return;
	}

	_active_socket.shutdown(boost::asio::ip::tcp::socket::shutdown_both);
	_active_socket.close();
}

PlayerSession::~PlayerSession()
{
	closeSocket();
}

boost::asio::deadline_timer::duration_type PlayerSession::deadline() const
{
	return _timer.expires_from_now();
	//return boost::asio::deadline_timer::duration_type();
}

void PlayerSession::waitForAMatch() 
{
	_timer.expires_from_now(boost::posix_time::seconds(wait_timeout));

	using boost::system::error_code;
	const auto callback = [this](const error_code& errorCode)
	{
		const auto canceled = [](const boost::system::error_code& errorCode)
		{
			return errorCode.value() == boost::system::errc::operation_canceled;
		};

		const auto isSingleAndOnline = [](const std::string& name)
		{
			return Manager::instance().isOnline(name) && !Manager::instance().hasMatch(name);
		};

		if (!canceled(errorCode) && isSingleAndOnline(name()))
			sendMessage("There is no suitable opponent. Please try later");
	};
	_timer.async_wait(callback);
}

void PlayerSession::cancelWaiting()
{
	_timer.cancel();
}

void PlayerSession::sendMessage(const std::string& message)
{
	std::ostream os(&_response);
	os << message << std::endl;
	write();
}

void PlayerSession::start()
{
	read();
}

void PlayerSession::read()
{
	auto self(shared_from_this());
	const auto handler = [this, self](const boost::system::error_code& errorCode, std::size_t /*bytesTransfered*/)
	{
		if (errorCode)
		{
			//TODO error handling please!
			return;
		}
		std::string line;
		std::istream is(&_request);
		std::getline(is, line);
		Manager::instance().parseCsv(self, line);
		//Manager::instance().logIn(self);
	};

	boost::asio::async_read_until(_active_socket, _request, "\n", handler);
}

void PlayerSession::write()
{
	const auto handler = [this](const boost::system::error_code& errorCode, std::size_t /*bytesTransfered*/)
	{
		if (errorCode)
		{
			//TODO error handling pelase!
			return;
		}
		if (_close_socket)
		{
			closeSocket();
			return;
		}
		read();
	};

	if (_close_socket)
	{
		auto self(shared_from_this());
		const auto keepSessionAliveUntilWrite = [this, self, handler](const boost::system::error_code& errorCode, std::size_t bytesTransfered)
		{
			handler(errorCode, bytesTransfered);
		};
		boost::asio::async_write(_active_socket, _response, keepSessionAliveUntilWrite);
	}
	else
		boost::asio::async_write(_active_socket, _response, handler);
}

Server::Server(short int port) :
	_acceptor(_io_context, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port)),
	_socket(_io_context)
{
	accept();
}

//TODO A user can establish a connection and then do nothing afterwards. A simple solution
//is to add a deadline timer to make sure a user cannot be idle more than a specific seconds
void Server::accept()
{
	const auto handler = [this](const boost::system::error_code& errorCode)
	{
		std::cout << "accepting a new connection" << std::endl;
		if (errorCode)
		{
			//TODO error handling please!
			std::cerr << "error in accpet handler" << std::endl;
			return;
		}
		std::make_shared<PlayerSession>(std::move(_socket))->start();
		accept();
	};

	_acceptor.async_accept(_socket, handler);
}

void Server::run()
{
	std::cout << "Listening for incoming messages" << std::endl;
	_io_context.run();
}
