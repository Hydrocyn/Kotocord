#ifndef APPPATHS_H
#define APPPATHS_H

#include <QString>
#include <QCoreApplication>
#include <QDir>

class AppPaths {
public:
	// 项目根目录 (自动适配开发和分发两种布局)
	static QString getProjectRootDir() {
		QString exeDir = QCoreApplication::applicationDirPath();
		// 分发模式: resources/ 在 exe 同级目录
		if (QDir(exeDir + "/resources").exists()) {
			return exeDir;
		}
		// 开发模式: exe 在 bin/Debug 或 bin/Release，项目根在 ../..
		return QDir::cleanPath(exeDir + "/../..");
	}

	//资源根目录
	static QString getResourcesDir() {
		return getProjectRootDir() + "/resources";
	}

	//具体的资源寻址方法 (对外暴露的 API)
	static QString getKaomojiPath() {
		return getResourcesDir() + "/kaomoji.json";
	}

	static QString getVoskModelPath() {
		return getResourcesDir() + "/model/vosk-model-small-cn-0.22";
	}

	static QString getWhisperModelPath() {
		return getResourcesDir() + "/model/ggml-small.bin";
	}

	// API Key 文件路径 (exe 同级目录，方便分发后用户编辑)
	static QString getApiKeyFilePath() {
		return QCoreApplication::applicationDirPath() + "/apikey.txt";
	}
};

#endif // APPPATHS_H
