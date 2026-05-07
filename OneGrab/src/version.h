#ifndef VERSION_H
#define VERSION_H

#define _DEBUG_ true

// gitee仓库账号名
const static QString GITEE_NAME = "dress_a";

// 项目名称
const static QString PROJECT_NAME = "one-grab";

#if _DEBUG_

#define APP_VERSION_MAJOR 0
#define APP_VERSION_MINOR 0
#define APP_VERSION_PATCH 0
#define APP_VERSION_BUILD 0
#define APP_VERSION_STR "0.0.0.0"

#else

// 版本号（FILEVERSION 由 RC 编译器使用，RC 不支持 constexpr，故使用 #else 回退）
#define APP_VERSION_MAJOR 1
#define APP_VERSION_MINOR 0

// 从 __DATE__ 自动计算编译日期版本号（仅在 C++ 编译器中生效）
// __DATE__ 格式："MMM DD YYYY"，例如 "May  7 2026"
#ifdef __cplusplus

constexpr int getYearLastTwo()
{
	return (__DATE__[9] - '0') * 10 + (__DATE__[10] - '0');
}

constexpr int getMonth()
{
	return (__DATE__[0] == 'J' && __DATE__[1] == 'a' && __DATE__[2] == 'n') ? 1 :
		(__DATE__[0] == 'F' && __DATE__[1] == 'e' && __DATE__[2] == 'b') ? 2 :
		(__DATE__[0] == 'M' && __DATE__[1] == 'a' && __DATE__[2] == 'r') ? 3 :
		(__DATE__[0] == 'A' && __DATE__[1] == 'p' && __DATE__[2] == 'r') ? 4 :
		(__DATE__[0] == 'M' && __DATE__[1] == 'a' && __DATE__[2] == 'y') ? 5 :
		(__DATE__[0] == 'J' && __DATE__[1] == 'u' && __DATE__[2] == 'n') ? 6 :
		(__DATE__[0] == 'J' && __DATE__[1] == 'u' && __DATE__[2] == 'l') ? 7 :
		(__DATE__[0] == 'A' && __DATE__[1] == 'u' && __DATE__[2] == 'g') ? 8 :
		(__DATE__[0] == 'S' && __DATE__[1] == 'e' && __DATE__[2] == 'p') ? 9 :
		(__DATE__[0] == 'O' && __DATE__[1] == 'c' && __DATE__[2] == 't') ? 10 :
		(__DATE__[0] == 'N' && __DATE__[1] == 'o' && __DATE__[2] == 'v') ? 11 : 12;
}

constexpr int getDay()
{
	return (__DATE__[4] == ' ') ? (__DATE__[5] - '0') : (__DATE__[4] - '0') * 10 + (__DATE__[5] - '0');
}

constexpr int APP_VERSION_PATCH = getYearLastTwo();
constexpr int APP_VERSION_BUILD = getMonth() * 100 + getDay();

const static QString APP_VERSION_STR = QStringLiteral("%1.%2.%3.%4")
	.arg(APP_VERSION_MAJOR)
	.arg(APP_VERSION_MINOR)
	.arg(APP_VERSION_PATCH)
	.arg(APP_VERSION_BUILD);

#else
// RC 编译器回退值（RC 不支持 constexpr）
#define APP_VERSION_PATCH 0
#define APP_VERSION_BUILD 0
#define APP_VERSION_STR "1.0.0.0"
#endif

#endif

#endif // VERSION_H
