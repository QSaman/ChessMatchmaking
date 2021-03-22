#include <boost/asio/connect.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/streambuf.hpp>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <utility>
#include <boost/asio/ts/buffer.hpp>
#include <boost/asio/ts/internet.hpp>
#include <boost/thread.hpp>
#include <thread>
#include <queue>

using Mutex = boost::shared_mutex;

Mutex mutex;

boost::asio::io_context io_context;

std::queue<std::string> request_queue;


class TcpClient
{
public:
	TcpClient(const boost::asio::ip::tcp::resolver::results_type& endpoints) :
	   	_socket(io_context)
	{
		connect(endpoints);
	}

	void sendRequest(const std::string& csvMessage)
	{
		const auto handler = [this, csvMessage]()
		{
			const bool write_in_progress = !request_queue.empty();
			request_queue.push(csvMessage);
			if (!write_in_progress)
				write();
		};

		boost::asio::post(io_context, handler);
	}

private:
	void connect(const boost::asio::ip::tcp::resolver::results_type& endpoints)
	{
		using boost::asio::ip::tcp;

		const auto handler = [this](const boost::system::error_code& errorCode, tcp::endpoint)
		{
			if (!errorCode)
			{
				read();
			}
		};

		boost::asio::async_connect(_socket, endpoints, handler);
	}

	void read()
	{
		const auto handler = [this](const boost::system::error_code& errorCode, std::size_t /*bytesTransfered*/)
		{
			if (errorCode)
			{
				return;
			}
			std::string line;
			std::istream is(&_response);
			std::getline(is, line);

			if (!line.empty())
			{
				boost::unique_lock<Mutex> guard(mutex);

				std::cout << line << std::endl;
			}
			read();
		};

		boost::asio::async_read_until(_socket, _response, "\n", handler);
	}

	void write()
	{
		const auto handler = [this](const boost::system::error_code& errorCode, std::size_t /*bytesTransfered*/)
		{
			if (errorCode)
			{
				return;
			}

			request_queue.pop();
			if (!request_queue.empty())
				write();
		};

		std::ostream os(&_request);
		os << request_queue.front() << std::flush;
		boost::asio::async_write(_socket, _request, handler);
	}

	boost::asio::ip::tcp::socket _socket;
	boost::asio::streambuf _request;
	boost::asio::streambuf _response;
};

class StdinClient
{
public:
	StdinClient(TcpClient& tcp) : _tcp(tcp)
	{
	}
	static void printMenu()
	{
		boost::unique_lock<Mutex> guard(mutex);

		std::cout << "Please choose one of these commands:" << std::endl;
		std::cout << "1. login" << std::endl;
		std::cout << "2. list_all" << std::endl;
		std::cout << "3. match" << std::endl;
		std::cout << "4. logout" << std::endl;
		std::cout << "Your choice: " << std::flush;
	}

	void readInput()
	{
		while (true)
		{
			printMenu();
			int choice;
			std::cin >> choice;
			switch (choice)
			{
				case 1:
					loginRequest();
					break;
				case 2:
					_tcp.sendRequest("list_all\n");
					break;
				case 3:
					_tcp.sendRequest("match\n");
					break;
				case 4:
					_tcp.sendRequest("logout\n");
					return;
				default:
					boost::unique_lock<Mutex> guard(mutex);
					std::cout << "Invalid option." << std::endl;
			}
		}
	}

	void loginRequest()
	{
		std::string command = "login,";

		{
			boost::unique_lock<Mutex> guard(mutex);

			std::cout << "Name: " << std::flush;
			std::string input;
			std::getline(std::cin, input); //discard \n
			std::getline(std::cin, input);
			command += input;
			command += ',';
			std::cout << "Country: " << std::flush;
			std::getline(std::cin, input);
			command += input;
			command += ',';
			std::cout << "Rating: " << std::flush;
			std::getline(std::cin, input);
			command += input;
			command += '\n';
		}

		std::cout << "before" << std::endl;
		_tcp.sendRequest(command);
		std::cout << "after" << std::endl;
	}
private:
	TcpClient& _tcp;
};


int main(int argc, char* argv[])
{
	const auto host = "localhost";
	const auto port = "7777";

	boost::asio::ip::tcp::resolver resolver(io_context);
	TcpClient tcp_client(resolver.resolve(host, port));
	StdinClient stdin_client(tcp_client);

	std::thread t([](){io_context.run();});
	stdin_client.readInput();
	t.join();
}
