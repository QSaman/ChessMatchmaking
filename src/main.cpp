#include <boost/asio/io_context.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/system/error_code.hpp>
#include <iostream>
#include <boost/asio.hpp>
#include <boost/optional.hpp>

#include "server.hpp"

void print(const boost::system::error_code&)
{
	std::cout << "Hello there!" << std::endl;
}

int main()
{
	Server::instance().run();
	return 0;
	boost::asio::io_context io;
	boost::asio::steady_timer t(io, boost::asio::chrono::seconds(5));
	t.async_wait(&print);
	const auto saman = [&t](const boost::system::error_code&)
	{
		std::cout << "All hail to Saman!" << std::endl;
		//t.cancel();
	};
	boost::optional<boost::asio::steady_timer> t3;
	t3 = std::move(t);
	t3.get().async_wait(saman);
	std::cout << "before run" << std::endl;
	io.run();
	std::cout << "after run" << std::endl;
}
