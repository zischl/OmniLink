
#include <OmniLogger.h>


void Logger::log(std::string_view text) {
	time(NULL);

	std::ofstream File("events.log.txt", std::ios::app);

	File << text;

	File.close();

#if defined(_DEBUG)
	std::cout << text << "\n";
#endif
}

void Logger::logHR(const HRESULT hr) {

	_com_error error(hr);
	LPCTSTR errorMsg = error.ErrorMessage();


	log("Error: {}" , errorMsg);
}

