#include "runner.cpp"

int main() {
	using namespace loadl;

	if(parse_duration("20") != 20) {
		return 1;
	}
	if(parse_duration("10:03") != 10*60+3) {
		return 2;
	}
	if(parse_duration("24:06:10") != 60*60*24+60*6+10) {
		return 3;
	}

	return 0;
}
