
#include <OmniLogger.h>


void Logger::log(std::string_view text) {
	time(NULL);

	std::ofstream File("events.log.txt", std::ios::app);

	File << text;

	File.close();
}


