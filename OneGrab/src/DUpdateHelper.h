#ifndef DUPDATEHELPER_H
#define DUPDATEHELPER_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVersionNumber>

class QNetworkAccessManager;
class QNetworkReply;
class QTimer;


class DUpdateHelper : public QObject
{
	Q_OBJECT

public:
	explicit DUpdateHelper(QObject* parent = nullptr);

	// 保存更新检查所需信息
	// owner: Gitee仓库所有者
	// repo: 仓库名称
	// currentVersion: 当前程序版本号，如 "1.2.3"
	void setInfos(const QString& owner,
		const QString& repo,
		const QString& currentVersion,
		const QString& accessToken = QString());

	// 设置定时检查间隔（毫秒）
	void setCheckInterval(int intervalMs);

	// 立即检查更新并启动定时器
	void startCheckTimer();

	// 停止定时器
	void stopCheckTimer();

	// 设置是否跳过预发布版本，默认跳过
	void setSkipPrerelease(bool skip);

	// 设置下载文件拓展名列表，默认只匹配 exe
	void setDownloadExtensions(const QStringList &extensions);

	// 主动调用一次更新检查（不启动定时器）
	void doCheck();

	// 下载文件
	// url: 下载地址
	// saveDir: 保存文件夹路径
	void downloadFile(const QString& url, const QString& saveDir);

signals:
	// 发现新版本
	void sigNewVersionAvailable(const QString& latestVersion,
		const QString& releaseUrl,
		const QString& releaseNotes,
		const QString& downloadUrl);
	// 已是最新版本
	void sigAlreadyLatest(const QString& currentVersion);
	// 检查失败
	void sigCheckFailed(const QString& errorMessage);
	// 下载完成
	void sigDownloadFinished(const QString& filePath);
	// 下载失败
	void sigDownloadFailed(const QString& errorMessage);
	// 下载进度（已接收字节, 总字节）
	void sigDownloadProgress(qint64 bytesReceived, qint64 bytesTotal);

private slots:
	void slotReplyFinished(QNetworkReply* reply);
	void slotDownloadFinished(QNetworkReply* reply);

private:
	struct ReleaseInfo
	{
		QString tagName;
		QVersionNumber versionNumber;
		QString htmlUrl;
		QString releaseNotes;
		QString downloadUrl;
	};

	QNetworkAccessManager* networkManager_;
	QNetworkAccessManager* downloadManager_;
	QString owner_;
	QString repo_;
	QString currentVersion_;
	QString accessToken_;
	QTimer* timer_;
	int checkIntervalMs_;
	bool skipPrerelease_;
	QStringList downloadExtensions_;

	ReleaseInfo getLatestRelease(const QJsonArray& releases);
	QVersionNumber parseVersionString(const QString& tagName);
	QString findBestDownloadUrl(const QJsonObject& release);
};

#endif // DUPDATEHELPER_H
