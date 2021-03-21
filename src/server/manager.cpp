#include "manager.hpp"

#include <functional>
#include <algorithm>

#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/classification.hpp>

namespace
{
	const std::string login_usage    = "login,name,country,rate";
	const std::string list_all_usage = "list_all";
	const std::string match_usage    = "match";
	const std::string logout_usage   = "logout";
}

Manager& Manager::instance()
{
	static Manager instance;
	return instance;
}

Manager::Manager()
{
	init();
}

bool Manager::isOnline(const std::string& name)
{
	ReadLock guard(_mutex);

	return _online_users.find(name) != _online_users.end();
}

bool Manager::hasMatch(const std::string& name)
{
	ReadLock guard(_mutex);

	return _match_list.find(name) != _match_list.end();
}

void Manager::parseCsv(std::shared_ptr<Player> player, const std::string& line)
{
	std::vector<std::string> tokens;
	boost::split(tokens, line, boost::is_any_of(","));
	if (tokens.empty() || _commands.find(tokens[0]) == _commands.end())
	{
		std::string msg = "Invalid command.";
		if (!isOnline(player->name()))
		{
			msg += " Please reconnect and first login using: ";
			msg += login_usage;
		}
		player->logout();
		player->sendMessage(msg);
		return;
	}
	if (tokens[0] != "login" && !isOnline(player->name()))
	{
		player->logout();
		player->sendMessage("You must first log in into the system. Please reconnect again.");
		return;
	}
	player->sendMessage(_commands[tokens[0]](player, ArgList(tokens.begin() + 1, tokens.end())));
}

void Manager::init()
{
	using std::placeholders::_1;
	using std::placeholders::_2;

	_commands["login"]    = std::bind(&Manager::login, this, _1, _2);
	_commands["list_all"] = std::bind(&Manager::listAll, this, _1, _2);
	_commands["match"]    = std::bind(&Manager::match, this, _1, _2);
	_commands["logout"]   = std::bind(&Manager::logout, this, _1, _2);
}

std::string Manager::login(std::shared_ptr<Player>& player, const ArgList& args)
{
	if (args.size() != 3)
	{
		std::string msg = "Invalid parameters. You should use ";
		msg += login_usage;
		return msg;
	}
	const auto& name = args[0];

	WriteLock guard(_mutex);

	//To avoid login as foo and bar in one session:
	const auto existingPlayerSession = [this](const auto& player)
	{
		return _online_users.find(player->name()) != _online_users.end();
	};

	if (_online_users.find(name) != _online_users.end() || existingPlayerSession(player))
		return "You've already logged in into the system!";
	const auto& country = args[1];
	//TODO check that it's a valid number:
	const auto rate = static_cast<std::size_t>(std::stoi(args[2]));
	player->setProfile(name, country, rate);
	_online_users[name] = player;
	_online_rates[rate].insert(name);
	_singles_rates[rate].insert(name);

	const auto match_res = noThreadSafeFindMatch(player, true);
	if (match_res.first)
	{
		noThreadSafeUpdateMatchCaches(player, match_res.second);
		auto opponent = _online_users[match_res.second];
		opponent->sendMessage("You've paired with " + player->toString());
		std::string msg = "You've successfully logged in: " + player->toString();
		msg += "\nYou've paired with " + opponent->toString();
		return msg;
	}
	return "You've successfully logged in: " + player->toString();
}

std::string Manager::listAll(std::shared_ptr<Player>& /*player*/, const ArgList& args)
{
	if (args.size() != 0)
	{
		std::string msg = "Invalid parameters. You should use ";
		msg += list_all_usage;
		return msg;
	}

	std::string res;
	bool first = true;

	ReadLock guard(_mutex);

	for (const auto& rate : _online_rates)
	{
		const auto rate_str = std::to_string(rate.first);
		for (const auto& name : rate.second)
		{
			if (first)
				first = false;
			else
				res += '\n';
			res += name;
			res += ", ";
			res += rate_str;
		}
	}
	return res;
}

std::pair<bool, std::string> Manager::noThreadSafeFindMatch(const std::shared_ptr<Player>& player, bool onlyInWaitList) const
{
	const auto findInList = [&player](const RateList& list)
	{
		const auto rating = player->rating();

		const auto min_rating = static_cast<std::size_t>(100);
		const auto max_rating = static_cast<std::size_t>(3000);
		const auto offset     = static_cast<std::size_t>(100);

		const auto min = std::max(min_rating , rating - offset);
		const auto max = std::min(max_rating, rating + offset);

		decltype(list.begin()) iter;

		for (std::size_t rate = max; rate >= min && (iter = list.find(rate)) == list.end(); --rate);

		if (iter == list.end())
			return std::string();

		for (; iter != list.end() && iter->first >= min; ++iter)
		{
			if (!iter->second.empty())
			{
				for (const auto& user : iter->second)
				{
					if (user != player->name())
						return user;
				}
			}
		}
		return std::string();
	};

	auto res = make_pair(false, std::string());

	const auto wait_res = findInList(_wait_list_rates);
	if (!wait_res.empty())
	{
		res.first = true, res.second = wait_res;
		const auto iter = _online_users.find(res.second);
		iter->second->cancelWaiting();
	}

	if (!res.first && !onlyInWaitList)
	{
		const auto singles_res = findInList(_singles_rates);
		if (!singles_res.empty())
			res.first = true, res.second = singles_res;
	}

	return res;
}

void Manager::noThreadSafeUpdateMatchCaches(const std::shared_ptr<Player>& player, const std::string& opponent)
{
	const auto opponent_player = _online_users[opponent];

	const auto update_match_caches = [this](const auto& player)
	{
		_wait_list_rates[player->rating()].erase(player->name());
		_singles_rates[player->rating()].erase(player->name());
	};

	update_match_caches(player);
	update_match_caches(opponent_player);

	_match_list[player->name()] = opponent;
	_match_list[opponent]       = player->name();
}

std::string Manager::match(std::shared_ptr<Player>& player, const ArgList& args)
{
	if (args.size() != 0)
	{
		std::string msg = "Invalid parameters. You should use ";
		msg += list_all_usage;
		return msg;
	}


	WriteLock guard(_mutex);

	const auto iter = _match_list.find(player->name());
	if (iter != _match_list.end())
		return "You cannot have more than 1 pair! Your current pair: " + _online_users[iter->second]->toString();

	const auto match_res = noThreadSafeFindMatch(player, false);

	if (!match_res.first)
	{
		_wait_list_rates[player->rating()].insert(player->name());
		_singles_rates[player->rating()].erase(player->name());
		player->waitForAMatch();
	    return "At the moment there is no suitable match. We'll let you know when one is avaialble in 60 seconds";
	}

	noThreadSafeUpdateMatchCaches(player, match_res.second);
	auto opponent = _online_users[match_res.second];
	opponent->sendMessage("You've paired with " + player->toString());
	return "You've paired with " + opponent->toString();
}

std::string Manager::logout(std::shared_ptr<Player>& player, const ArgList& args)
{
	if (args.size() != 0)
	{
		std::string msg = "Invalid parameters. You should use ";
		msg += logout_usage;
		return msg;
	}

	WriteLock guard(_mutex);

	auto iter = _online_users.find(player->name());

	auto match_iter = _match_list.find(player->name()); 
	if (match_iter != _match_list.end())
	{
		auto opponent_player = _online_users[match_iter->second];
		opponent_player->sendMessage("Your opponent logged out from the system: " + player->toString());

		_match_list.erase(match_iter);
		_match_list.erase(opponent_player->name());
		_singles_rates[opponent_player->rating()].insert(opponent_player->name());
	}

	_online_rates[player->rating()].erase(player->name());
	player->logout();
	//Note that PlayerSession::read's handler keep a shared ptr. So it's safe to remove it:
	_online_users.erase(iter);

	return "You've successfully logged out from the system: " + player->toString();
}
