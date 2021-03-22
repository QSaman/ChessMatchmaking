#include <boost/asio/connect.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/streambuf.hpp>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <boost/asio/ts/buffer.hpp>
#include <boost/asio/ts/internet.hpp>
#include <boost/thread.hpp>
#include <thread>
#include <queue>
#include <condition_variable>
#include <mutex>

using Mutex = std::mutex;

Mutex mutex;

std::condition_variable cv;

boost::asio::io_context io_context;

std::queue<std::string> request_queue;


bool cli_continue;

class TcpClient
{
public:
	TcpClient(const boost::asio::ip::tcp::resolver::results_type& endpoints) :
	   	_socket(io_context)
	{
		cli_continue = true;
		connect(endpoints);
	}

	void sendRequest(const std::string& csvMessage)
	{
		{
			std::unique_lock<Mutex> guard(mutex);
			cli_continue = false;
		}
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
			std::unique_lock<Mutex> guard(mutex);

			std::string line;
			std::istream is(&_response);

			std::cout << std::endl;
			std::cout << "*********" << std::endl;

			while (std::getline(is, line))
				std::cout << line << std::endl;

			std::cout << "*********" << std::endl;
			cli_continue = true;
			cv.notify_all();
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
		std::unique_lock<Mutex> guard(mutex);

		std::cout << std::endl;
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
			using namespace std::chrono_literals;
			{
				std::unique_lock<Mutex> guard(mutex);
				while (!cli_continue)
					cv.wait(guard);
			}
			printMenu();
			int choice;
			std::string choice_str;

			std::getline(std::cin, choice_str);
			try
			{
				choice = std::stoi(choice_str);
			}
			catch(...)
			{
				std::unique_lock<Mutex> guard(mutex);
				std::cout << "Invalid choice. Please enter a number between 1 and 4" << std::endl;
				continue;
			}
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
					std::unique_lock<Mutex> guard(mutex);
					std::cout << "Invalid option." << std::endl;
			}
		}
	}

	void loginRequest()
	{
		std::string command = "login,";

		{
			std::unique_lock<Mutex> guard(mutex);

			std::cout << "Name: " << std::flush;
			std::string input;
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

		_tcp.sendRequest(command);
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
