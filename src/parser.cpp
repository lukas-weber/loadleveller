#include "parser.h"
#include <fstream>

namespace loadl {

parser::iterator::iterator(std::string filename, json::iterator it)
    : filename_{std::move(filename)}, it_{std::move(it)} {}

std::pair<std::string, parser> parser::iterator::operator*() {
	return std::make_pair(it_.key(), parser{it_.value(), filename_});
}

static std::runtime_error non_map_error(const std::string &filename) {
	return std::runtime_error(
	    fmt::format("json: {}: trying to dereference non-map node.", filename));
}

static std::runtime_error key_error(const std::string &filename, const std::string &key) {
	return std::runtime_error(
	    fmt::format("json: {}: could not find required key '{}'", filename, key));
}

parser::iterator parser::iterator::operator++() {
	return iterator{filename_, it_++};
}

bool parser::iterator::operator!=(const iterator &b) {
	return it_ != b.it_;
}

parser::parser(const json &node, const std::string &filename)
    : content_(node), filename_{filename} {
	if(!content_.is_object()) {
		throw non_map_error(filename);
	}
}

parser::parser(const std::string &filename) : filename_{filename} {
	std::ifstream f(filename);
	f >> content_;
	if(!content_.is_object()) {
		throw non_map_error(filename);
	}
}

parser::iterator parser::begin() {
	if(!content_.is_object()) {
		throw non_map_error(filename_);
	}

	return iterator{filename_, content_.begin()};
}

parser::iterator parser::end() {
	return iterator{filename_, content_.end()};
}

bool parser::defined(const std::string &key) const {
	if(!content_.is_object()) {
		return false;
	}
	return content_.find(key) != content_.end();
}

parser parser::operator[](const std::string &key) {
	if(!content_.is_object()) {
		throw non_map_error(filename_);
	}

	auto node = content_[key];
	if(node.is_null()) {
		throw key_error(filename_, key);
	}
	if(!node.is_object()) {
		throw std::runtime_error(fmt::format(
		    "json: {}: Found key '{}', but it has a scalar value. Was expecting it to be a map",
		    filename_, key));
	}

	return parser{node, filename_};
}

const json &parser::get_json() const {
	return content_;
}
}
