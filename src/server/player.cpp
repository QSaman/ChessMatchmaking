#include "player.hpp"

#include <sstream>

void Player::setProfile(const std::string& name, const std::string& country, const std::size_t rating)
{
	_name = name;
	_country = country;
	_rating = rating;
}

boost::asio::deadline_timer::duration_type Player::deadline() const
{
	return boost::asio::deadline_timer::duration_type();
}

std::string Player::toString() const
{
	std::ostringstream oss;
	oss << "name: " << _name << ", country: " << _country << ", rating: " << _rating;
	return oss.str();
}
