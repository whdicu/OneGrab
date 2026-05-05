#ifndef DUPDATEHELPER_H
#define DUPDATEHELPER_H

#include <QObject>
#include <QString>
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

	// 主动调用一次更新检查（不启动定时器）
	void doCheck();

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

private slots:
	void slotReplyFinished(QNetworkReply* reply);

private:
	struct ReleaseInfo
	{
		QString tagName;
		QVersionNumber versionNumber;
		QString releaseUrl;
		QString releaseNotes;
		QString downloadUrl;
	};

	QNetworkAccessManager* networkManager_;
	QString owner_;
	QString repo_;
	QString currentVersion_;
	QString accessToken_;
	QTimer* timer_;
	int checkIntervalMs_;
	bool skipPrerelease_;

	ReleaseInfo getLatestRelease(const QJsonArray& releases);
	QVersionNumber parseVersionString(const QString& tagName);
	QString findBestDownloadUrl(const QJsonObject& release);
};

#endif // DUPDATEHELPER_H
