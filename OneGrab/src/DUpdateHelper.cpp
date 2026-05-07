#include "DUpdateHelper.h"
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QTimer>
#include <QVersionNumber>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QDebug>

DUpdateHelper::DUpdateHelper(QObject *parent)
	: QObject(parent)
	, checkIntervalMs_(1000 * 60 * 60)  // 默认一小时
	, skipPrerelease_(true)
	, downloadExtensions_({"zip"})
{
	networkManager_ = new QNetworkAccessManager(this);
	connect(networkManager_, &QNetworkAccessManager::finished,
		this, &DUpdateHelper::slotReplyFinished);

	downloadManager_ = new QNetworkAccessManager(this);
	connect(downloadManager_, &QNetworkAccessManager::finished,
		this, &DUpdateHelper::slotDownloadFinished);

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

void DUpdateHelper::setDownloadExtensions(const QStringList &extensions)
{
	downloadExtensions_ = extensions;
}

void DUpdateHelper::downloadFile(const QString &url, const QString &saveDir)
{
	// 从URL中提取文件名
	QString fileName = QUrl(url).fileName();
	if (fileName.isEmpty())
		fileName = "update_file";

	// 确保保存目录存在
	QDir dir(saveDir);
	if (!dir.exists())
		dir.mkpath(".");

	QString savePath = dir.filePath(fileName);

	qDebug() << "Downloading file:" << url;
	qDebug() << "Save to:" << savePath;

	QNetworkRequest request = QNetworkRequest(QUrl(url));
	request.setRawHeader("Accept", "application/octet-stream");

	// 发起请求并连接进度信号
	QNetworkReply* reply = downloadManager_->get(request);
	reply->setProperty("savePath", savePath);
	connect(reply, &QNetworkReply::downloadProgress,
		this, &DUpdateHelper::sigDownloadProgress);
}

void DUpdateHelper::slotDownloadFinished(QNetworkReply* reply)
{
	QString savePath = reply->property("savePath").toString();
	QString url = reply->url().toString();
	QByteArray data = reply->readAll();
	QString sss = data;

	if (reply->error() != QNetworkReply::NoError)
	{
		qDebug() << "Download failed:" << url << reply->errorString();
		emit sigDownloadFailed(reply->errorString());
		reply->deleteLater();
		return;
	}

	// 检查是否是 HTML 重定向页面（Gitee 附件链接会返回这种页面）
	QString contentType = reply->header(QNetworkRequest::ContentTypeHeader).toString();
	if (contentType.contains("text/html") || data.startsWith("<html"))
	{
		QString html = QString::fromUtf8(data);
		QString redirectUrl;

		// 从 <a href="..."> 中提取真实下载地址
		int hrefStart = html.indexOf("href=\"");
		if (hrefStart >= 0)
		{
			hrefStart += 6;  // 跳过 href="
			int hrefEnd = html.indexOf("\"", hrefStart);
			if (hrefEnd > hrefStart)
				redirectUrl = html.mid(hrefStart, hrefEnd - hrefStart);
		}

		if (!redirectUrl.isEmpty())
		{
			// 防止无限重定向
			int redirectCount = reply->property("redirectCount").toInt();
			if (redirectCount > 5)
			{
				emit sigDownloadFailed("Too many redirects");
				reply->deleteLater();
				return;
			}

			// 解码 HTML 实体（如 &amp; → &）
			redirectUrl.replace("&amp;", "&");

			qDebug() << "Redirect to:" << redirectUrl;

			QNetworkRequest request = QNetworkRequest(QUrl(redirectUrl));
			QNetworkReply* newReply = downloadManager_->get(request);
			newReply->setProperty("savePath", savePath);
			newReply->setProperty("redirectCount", redirectCount + 1);
			connect(newReply, &QNetworkReply::downloadProgress,
				this, &DUpdateHelper::sigDownloadProgress);
			reply->deleteLater();
			return;
		}
	}

	// 正常文件数据，写入文件
	QFile file(savePath);
	if (file.open(QIODevice::WriteOnly))
	{
		file.write(data);
		file.close();
		qDebug() << "Download finished:" << savePath;
		emit sigDownloadFinished(savePath);
	}
	else
	{
		qDebug() << "Failed to write file:" << savePath;
		emit sigDownloadFailed(QStringLiteral("无法写入文件: %1").arg(savePath));
	}

	reply->deleteLater();
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
	QString sss = responseData;
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
			latest.htmlUrl,
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

		info.htmlUrl = release.value("html_url").toString();
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
	for (const QJsonValue &assetValue : assets)
	{
		QJsonObject asset = assetValue.toObject();
		QString assetName = asset.value("name").toString();

		QString downloadUrl = asset.value("browser_download_url").toString();

		// 优先检查URL中是否包含"releases"关键词
		if (!downloadUrl.contains("releases", Qt::CaseInsensitive))
			continue;

		// 再匹配文件拓展名
		for (const QString &ext : downloadExtensions_)
		{
			if (assetName.endsWith("." + ext))
			{
				return downloadUrl;
			}
		}
	}

	// 如果没有合适的附件，返回Release页面URL
	return release.value("html_url").toString();
}
