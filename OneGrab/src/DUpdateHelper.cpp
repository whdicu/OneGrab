#include "DUpdateHelper.h"
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QTimer>
#include <QVersionNumber>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QDebug>

DUpdateHelper::DUpdateHelper(QObject *parent)
	: QObject(parent)
	, checkIntervalMs_(1000 * 60 * 60)  // 默认一小时
	, skipPrerelease_(true)
{
	networkManager_ = new QNetworkAccessManager(this);
	connect(networkManager_, &QNetworkAccessManager::finished,
		this, &DUpdateHelper::slotReplyFinished);

	timer_ = new QTimer(this);
	connect(timer_, &QTimer::timeout,
		this, &DUpdateHelper::doCheck);
}

void DUpdateHelper::setInfos(const QString &owner,
	const QString &repo,
	const QString &currentVersion,
	const QString &accessToken)
{
	owner_ = owner;
	repo_ = repo;
	currentVersion_ = currentVersion;
	accessToken_ = accessToken;
}

void DUpdateHelper::setCheckInterval(int intervalMs)
{
	checkIntervalMs_ = intervalMs;
}

void DUpdateHelper::startCheckTimer()
{
	doCheck();
	timer_->start(checkIntervalMs_);
}

void DUpdateHelper::stopCheckTimer()
{
	timer_->stop();
}

void DUpdateHelper::setSkipPrerelease(bool skip)
{
	skipPrerelease_ = skip;
}

void DUpdateHelper::doCheck()
{
	// 构造API请求URL
	QString apiUrl = QString(
		"https://gitee.com/api/v5/repos/%1/%2/releases"
	).arg(owner_, repo_);

	QNetworkRequest request = QNetworkRequest(QUrl(apiUrl));
	request.setRawHeader("Accept", "application/json");

	// 如果有access token，添加到请求中（提高API调用频率限制）
	if (!accessToken_.isEmpty())
	{
		request.setRawHeader("Authorization",
			("Bearer " + accessToken_).toUtf8());
	}

	networkManager_->get(request);
}

void DUpdateHelper::slotReplyFinished(QNetworkReply* reply)
{
	if (reply->error() != QNetworkReply::NoError)
	{
		emit sigCheckFailed(reply->errorString());
		reply->deleteLater();
		return;
	}

	QByteArray responseData = reply->readAll();
	reply->deleteLater();

	// 解析JSON数组
	QJsonDocument jsonDoc = QJsonDocument::fromJson(responseData);
	if (!jsonDoc.isArray()) {
		emit sigCheckFailed("Invalid JSON response: expected an array of releases");
		return;
	}

	QJsonArray releases = jsonDoc.array();
	if (releases.isEmpty()) {
		emit sigCheckFailed("No releases found in this repository");
		return;
	}

	// 获取最新版本
	ReleaseInfo latest = getLatestRelease(releases);
	if (latest.versionNumber.isNull()) {
		emit sigCheckFailed("Failed to parse version from releases");
		return;
	}

	// 比较版本号
	QVersionNumber currentV = parseVersionString(currentVersion_);
	if (currentV.isNull())
	{
		emit sigCheckFailed("Invalid current version format");
		return;
	}

	qDebug() << "Check update: latest version =" << latest.tagName
		<< ", current version =" << currentVersion_;

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
	int cmp = latest.versionNumber.compare(currentV); // Qt 6.x
#else
	int cmp = QVersionNumber::compare(latest.versionNumber, currentV);
#endif

	if (cmp > 0) {
		emit sigNewVersionAvailable(latest.tagName,
			latest.releaseUrl,
			latest.releaseNotes,
			latest.downloadUrl);
	}
	else {
		emit sigAlreadyLatest(currentVersion_);
	}
}

DUpdateHelper::ReleaseInfo DUpdateHelper::getLatestRelease(
	const QJsonArray &releases)
{
	QList<ReleaseInfo> allReleases;

	for (const QJsonValue &value : releases) {
		QJsonObject release = value.toObject();

		// 跳过预发布版本
		if (skipPrerelease_ && release.value("prerelease").toBool()) {
			continue;
		}

		// 跳过草稿版本
		if (release.value("draft").toBool()) {
			continue;
		}

		ReleaseInfo info;
		info.tagName = release.value("tag_name").toString();
		info.versionNumber = parseVersionString(info.tagName);

		if (info.versionNumber.isNull())
			continue;

		info.releaseUrl = release.value("html_url").toString();
		info.releaseNotes = release.value("body").toString();
		info.downloadUrl = findBestDownloadUrl(release);

		allReleases.append(info);
	}

	// 按版本号排序，找出最新的
	std::sort(allReleases.begin(), allReleases.end(),
		[](const ReleaseInfo &a, const ReleaseInfo &b) {
		return a.versionNumber > b.versionNumber;
	});

	return allReleases.isEmpty() ? ReleaseInfo() : allReleases.first();
}

QVersionNumber DUpdateHelper::parseVersionString(const QString &tagName)
{
	QString version = tagName;

	// 去掉可能的前缀'v'或'V'
	if (version.startsWith('v') || version.startsWith('V')) {
		version = version.mid(1);
	}

	// 使用QVersionNumber解析
	return QVersionNumber::fromString(version);
}

QString DUpdateHelper::findBestDownloadUrl(const QJsonObject &release)
{
	// 优先查找assets（附件）
	QJsonArray assets = release.value("assets").toArray();
	for (const QJsonValue &assetValue : assets) {
		QJsonObject asset = assetValue.toObject();
		QString assetName = asset.value("name").toString();

		// 根据文件扩展名筛选（可根据实际需求调整）
		if (assetName.endsWith(".exe") || assetName.endsWith(".AppImage")) {
			return asset.value("browser_download_url").toString();
		}
	}

	// 如果没有合适的附件，返回Release页面URL
	return release.value("html_url").toString();
}
