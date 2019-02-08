#include "parser.h"

parser::iterator::iterator(std::string filename, YAML::Node::iterator it)
	: filename_{std::move(filename)}, it_{std::move(it)} {
}

std::pair<std::string, parser> parser::iterator::operator*() {
	try {
		return std::make_pair(it_->first.as<std::string>(), parser{it_->second, filename_});
	} catch(YAML::Exception& e) {
		throw std::runtime_error(fmt::format("YAML: {}: dereferencing map key failed: {}. Maybe it was not a string?", filename_, e.what()));
	}
}

static std::runtime_error non_map_error(const std::string& filename) {
	return std::runtime_error(fmt::format("YAML: {}: trying to dereference non-map node.", filename));
}

static std::runtime_error key_error(const std::string& filename, const std::string& key) {
	return std::runtime_error(fmt::format("YAML: {}: could not find required key '{}'", filename, key));
}


parser::iterator parser::iterator::operator++() {
	return iterator{filename_, it_++};
}

bool parser::iterator::operator!=(const iterator& b) {
	return it_ != b.it_;
}

parser::parser(const YAML::Node& node, const std::string& filename) 
	: content_{node}, filename_{filename} {
	if(!content_.IsMap()) {
		throw non_map_error(filename);
	}
}

parser::parser(const std::string& filename)
	: parser{YAML::LoadFile(filename), filename} {
}

parser::iterator parser::begin() {
	if(!content_.IsMap()) {
		throw non_map_error(filename_);
	}

	return iterator{filename_, content_.begin()};
}

parser::iterator parser::end() {

	return iterator{filename_, content_.end()};
}

bool parser::defined(const std::string& key) {
	if(!content_.IsMap()) {
		return false;
	}
	return content_[key].IsDefined();
}

parser parser::operator[](const std::string& key) {
	if(!content_.IsMap()) {
		throw non_map_error(filename_);
	}
		
	auto node = content_[key];
	if(!node.IsDefined()) {
		throw key_error(filename_, key);
	}
	if(!node.IsMap()) {
		throw std::runtime_error(fmt::format("YAML: {}: Found key '{}', but it has a scalar value. Was expecting it to be a map", filename_, key));
	}

	try {
		return parser{node, filename_};
	} catch(YAML::Exception&) {
		throw key_error(filename_, key);
	}
}

const YAML::Node& parser::get_yaml() {
	return content_;
}
