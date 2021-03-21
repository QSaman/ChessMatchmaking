#pragma once

#include <string>
#include <unordered_map>
#include <memory>
#include <vector>
#include <map>
#include <set>
#include <utility>

#include "player.hpp"

#include <boost/thread.hpp>

class Manager
{
public:
	using ArgList = std::vector<std::string>;

	static Manager& instance();
	
	Manager();
	void parseCsv(std::shared_ptr<Player> player, const std::string& line);
	bool isOnline(const std::string& name);
	bool hasMatch(const std::string& name);


	std::string login(std::shared_ptr<Player>& player, const ArgList& args);
	std::string listAll(std::shared_ptr<Player>& player, const ArgList& args);
	std::string match(std::shared_ptr<Player>& player, const ArgList& args);
	std::string logout(std::shared_ptr<Player>& player, const ArgList& args);

private:
	void init();
	std::pair<bool, std::string> noThreadSafeFindMatch(const std::shared_ptr<Player>& player, bool onlyInWaitList) const;
	void noThreadSafeUpdateMatchCaches(const std::shared_ptr<Player>& player, const std::string& opponent);

	using Mutex = boost::shared_mutex;
	using ReadLock = boost::shared_lock<Mutex>;
	using WriteLock = boost::unique_lock<Mutex>;

	Mutex _mutex;

	using CommandHandler = std::function<std::string (std::shared_ptr<Player>&, const ArgList&)>;
	using CommandList = std::unordered_map<std::string, CommandHandler>;

	using OnlineList = std::unordered_map<std::string, std::shared_ptr<Player>>;
	using RateList = std::map<std::size_t, std::set<std::string>, std::greater<std::size_t>>;
	using MatchList = std::unordered_map<std::string, std::string>;

	OnlineList _online_users;	//All online users

	RateList _online_rates;		//All online users
	RateList _wait_list_rates;	//All users who are waiting for a match
	RateList _singles_rates;	//All online users who don't request for a match

	MatchList _match_list;

	CommandList _commands;
};
