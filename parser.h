#pragma once

#include <yaml-cpp/yaml.h>
#include <fmt/format.h>

// This is mostly a wrapper around yaml-cpp with more helpful error handling.
// For simplicity it does not support advanced yaml features such as complex-typed
// keys in maps.

class parser {
private:
	YAML::Node content_;
	const std::string filename_;
	
	// fake parser based on a subnode
	parser(const YAML::Node& node, const std::string& filename);
public:
	class iterator {
	private:
		std::string filename_;
		YAML::Node::iterator it_;
	public:
		iterator(std::string filename, YAML::Node::iterator it);
		std::pair<std::string, parser> operator*();
		iterator operator++();
		bool operator!=(const iterator& b);
	};
	
	parser(const std::string& filename);

	template<typename T>
	T get(const std::string& name) {
		if(!content_[name]) {
			throw std::runtime_error(fmt::format("YAML: {}: required key '{}' not found.", filename_, name));
		}
		return content_[name].as<T>();
	}
	
	template<typename T>
	auto get(const std::string& name, T default_val) -> decltype(default_val) {
		return content_[name].as<T>(default_val);
	}

	// is key name defined?
	bool defined(const std::string& name);

	parser operator[](const std::string& name);
	iterator begin();
	iterator end();

	// This gives access to the underlying yaml-cpp api. Only use it if you absolutely need to.
	// This function is needed to dump the task settings into the result file for example.
	const YAML::Node& get_yaml();
};
