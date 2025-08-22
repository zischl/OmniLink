
#include <OmniLogger.h>

using namespace std;

void Logger::log(string text) {
	ofstream File("events.log.txt", ios::app);

	File << text;

	File.close();
}