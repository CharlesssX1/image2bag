#ifndef UTILS_H
#define UTILS_H

#include <string>

struct Config {
	bool uploadOn;
	std::string obsBucketDir;
	std::string obsHostName;
	std::string obsBucketName;
	std::string obsAccessKey;
	std::string obsSecretKey;
	std::string recordingLocalDir;
};

int InitConf(Config &conf);


#endif //UTILS_H
