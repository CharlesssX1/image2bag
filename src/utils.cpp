#include "utils.h"
#include "yaml-cpp/yaml.h"
#include <iostream>
 
namespace {
	const std::string UPLOAD_ON = "upload_on";
	const std::string OBS_BUCKET_DIR = "obs_bucket_dir";
	const std::string OBS_BUCKET_NAME = "obs_bucket_name";
	const std::string OBS_HOST_NAME = "obs_host_name";
	const std::string OBS_ACCESS_KEY = "obs_access_key";
	const std::string OBS_SECRET_KEY = "obs_secret_key";
	const std::string RECORDING_LOCAL_DIR = "recording_local_dir";
}

std::string GetEnv(const std::string &envKey) {
	const char *val = std::getenv(envKey.c_str());
	if (val == nullptr) {
		return "";
	}
	return std::string(val);
}

int InitConf(Config &conf) {
	try { 
		YAML::Node yamlConf = YAML::LoadFile("./config.yaml");
		if (!yamlConf[UPLOAD_ON].IsNull()) {
			conf.uploadOn = yamlConf[UPLOAD_ON].as<bool>();
		} else {
			conf.uploadOn = false;
			std::cout << "config UPLOAD_ON is empty, please check config, upload is currently off." << std::endl;
		}

		if (!yamlConf[OBS_BUCKET_DIR].IsNull()) {
			conf.obsBucketDir = yamlConf[OBS_BUCKET_DIR].as<std::string>();
		} else {
			conf.obsBucketDir = GetEnv(OBS_BUCKET_DIR);
		}
		if (conf.obsBucketDir.empty()) {
			std::cout << "config OBS_BUCKET_DIR is empty, please check config." << std::endl;
			return -1;
		}
		
		if (!yamlConf[OBS_BUCKET_NAME].IsNull()) {
			conf.obsBucketName = yamlConf[OBS_BUCKET_NAME].as<std::string>();
		} else {
			conf.obsBucketName = GetEnv(OBS_BUCKET_NAME);
		}
		if (conf.obsBucketName.empty()) {
			std::cout << "config OBS_BUCKET_NAME is empty, please check config." << std::endl;
			return -1;
		}
		
		if (!yamlConf[OBS_HOST_NAME].IsNull()) {
			conf.obsHostName = yamlConf[OBS_HOST_NAME].as<std::string>();
		} else {
			conf.obsHostName = GetEnv(OBS_HOST_NAME);
		}
		if (conf.obsHostName.empty()) {
			std::cout << "config OBS_HOST_NAME is empty, please check config." << std::endl;
			return -1;
		}
		
		if (!yamlConf[OBS_ACCESS_KEY].IsNull()) {
			conf.obsAccessKey = yamlConf[OBS_ACCESS_KEY].as<std::string>();
		} else {
			conf.obsAccessKey = GetEnv(OBS_ACCESS_KEY);
		}
		if (conf.obsAccessKey.empty()) {
			std::cout << "config OBS_ACCESS_KEY is empty, please check config." << std::endl;
			return -1;
		}
		
		if (!yamlConf[OBS_SECRET_KEY].IsNull()) {
			conf.obsSecretKey = yamlConf[OBS_SECRET_KEY].as<std::string>();
		} else {
			conf.obsSecretKey = GetEnv(OBS_SECRET_KEY);
		}
		if (conf.obsSecretKey.empty()) {
			std::cout << "config OBS_SECRET_KEY is empty, please check config." << std::endl;
			return -1;
		}
		
		if (!yamlConf[RECORDING_LOCAL_DIR].IsNull()) {
			conf.recordingLocalDir = yamlConf[RECORDING_LOCAL_DIR].as<std::string>();
		} else {
			conf.recordingLocalDir = GetEnv(RECORDING_LOCAL_DIR);
		}
		if (conf.recordingLocalDir.empty()) {
			std::cout << "config RECORDING_LOCAL_DIR is empty, please check config." << std::endl;
			return -1;
		}
	} catch (const YAML::Exception& e) {
		std::cerr << "Error while parsing YAML: " << e.what() << std::endl;
	}
	
	std::cout << "================== Config ===================" << std::endl;
	std::cout << "upload_on: " << conf.uploadOn << std::endl;
	std::cout << "obs_bucket_dir: " << conf.obsBucketDir << std::endl;
	std::cout << "obs_bucket_name: " << conf.obsBucketName << std::endl;
	std::cout << "obs_host_name: " << conf.obsHostName << std::endl;
	std::cout << "obs_access_key: " << conf.obsAccessKey << std::endl;
	std::cout << "obs_secret_key: " << conf.obsSecretKey << std::endl;
	std::cout << "recording_local_dir: " << conf.recordingLocalDir << std::endl;
	std::cout << "=============================================" << std::endl;
	return 0;
}
