#pragma once

#include <boost/asio/deadline_timer.hpp>
#include <string>

class Player
{
public:
	Player() = default;
	void setProfile(const std::string& name, const std::string& country, std::size_t rating);
	virtual void waitForAMatch() {};
	virtual void cancelWaiting() {};
	virtual void sendMessage(const std::string& message) {}
	virtual boost::asio::deadline_timer::duration_type deadline() const;
	virtual void logout() {};
	virtual ~Player() = default;

	std::string toString() const;
	const std::string& name() const {return _name;}
	std::size_t rating() const {return _rating;}
private:
	std::string _name;
	std::string _country;
	std::size_t _rating; //Assume it's an integer between 50 and 3000
};
