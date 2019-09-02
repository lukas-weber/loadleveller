#pragma once

#include <fmt/format.h>
#include <nlohmann/json.hpp>

namespace loadl {

// This is mostly a wrapper around yaml-cpp with more helpful error handling.
// For simplicity it does not support advanced yaml features such as complex-typed
// keys in maps.

using json = nlohmann::json;

class parser {
private:
	json content_;
	const std::string filename_;

	// fake parser based on a subnode
	parser(const json &node, const std::string &filename);

public:
	class iterator {
	private:
		std::string filename_;
		json::iterator it_;

	public:
		iterator(std::string filename, json::iterator it);
		std::pair<std::string, parser> operator*();
		iterator operator++();
		bool operator!=(const iterator &b);
	};

	parser(const std::string &filename);

	template<typename T>
	T get(const std::string &key) const {
		auto v = content_.find(key);
		if(v == content_.end()) {
			throw std::runtime_error(
			    fmt::format("json: {}: required key '{}' not found.", filename_, key));
		}
		return *v;
	}

	template<typename T>
	auto get(const std::string &key, T default_val) const {
		return content_.value<T>(key, default_val);
	}

	// is key defined?
	bool defined(const std::string &key) const;

	parser operator[](const std::string &key);
	iterator begin();
	iterator end();

	// This gives access to the underlying yaml-cpp api. Only use it if you absolutely need to.
	// This function is needed to dump the task settings into the result file for example.
	const json &get_json() const;
};
}
