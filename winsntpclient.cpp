#include "stdafx.h"
#include "resource.h"
#include "winsock.h"
#include "stdio.h"
#include "time.h"

#define APP_NAME "WinSNTPClient"
#define NTP_SERVER "ptbtime1.ptb.de"
#define RECEIVE_TIMEOUT_MS 2000
#define NTP_TIME_OFFSET 2208988800

typedef struct {
	unsigned char flags;
	unsigned char peerClockStratum;
	unsigned char peerPollingInterval;
	unsigned char peerClockPrecision;
	unsigned int rootDelay;
	unsigned int rootDispersion;
	unsigned char referenceId[4];
	unsigned int referenceTimestamp;
	unsigned int referenceTimestampFractional;
	unsigned int originTimestamp;
	unsigned int originTimestampFractional;
	unsigned int receiveTimestamp;
	unsigned int receiveTimestampFractional;
	unsigned int transmitTimestamp;
	unsigned int transmitTimestampFractional;
} NtpPacket;

bool mapNtpTimeToSystemTime(unsigned int ntpTime, SYSTEMTIME* st) {
	time_t ntpTimeTime = ntpTime - NTP_TIME_OFFSET;

	struct tm* utcTime = gmtime(&ntpTimeTime);
	if (!utcTime) {
		return FALSE;
	}

	st->wYear = (unsigned short)(utcTime->tm_year + 1900);
	st->wMonth = (unsigned short)(utcTime->tm_mon + 1);
	st->wDay = (unsigned short)utcTime->tm_mday;
	st->wHour = (unsigned short)utcTime->tm_hour;
	st->wMinute = (unsigned short)utcTime->tm_min;
	st->wSecond = (unsigned short)utcTime->tm_sec;
	st->wMilliseconds = 0;

	return TRUE;
}

int APIENTRY WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
	char message[512];

	if (MessageBox(0, "Synchronize system clock now using SNTP?", APP_NAME, MB_YESNO | MB_ICONQUESTION) == IDNO) {
		return TRUE;
	}

	WSADATA wsaData;
	int startupResult = WSAStartup(0x101, &wsaData);
	if (startupResult != 0) {
		sprintf(message, "Unable to initialize WinSock: %i", startupResult);
		MessageBox(0, message, APP_NAME, MB_OK | MB_ICONEXCLAMATION);

		return FALSE;
	}

	struct hostent *h = gethostbyname(NTP_SERVER);
	if (h == NULL) {
		sprintf(message, "Unable to resolve hostname: %i", WSAGetLastError());
		MessageBox(0, message, APP_NAME, MB_OK | MB_ICONEXCLAMATION);
		WSACleanup();

		return FALSE;
	}

	struct sockaddr_in a;
	a.sin_family = AF_INET;
	a.sin_port = htons(123);
	a.sin_addr.s_addr = *(unsigned long*)h->h_addr;
	
	int s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (s == INVALID_SOCKET) {
		sprintf(message, "Unable to create socket: %i", WSAGetLastError());
		MessageBox(0, message, APP_NAME, MB_OK | MB_ICONEXCLAMATION);
		WSACleanup();

		return FALSE;
	}

	DWORD timeout = RECEIVE_TIMEOUT_MS;
	if (setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout)) == SOCKET_ERROR) {
		sprintf(message, "Unable to set socket timeout: %i", WSAGetLastError());
		MessageBox(0, message, APP_NAME, MB_OK | MB_ICONEXCLAMATION);
		WSACleanup();

		return FALSE;
	}

	NtpPacket packet; // used for sending and receiving
	memset((void *)&packet, 0, sizeof(packet));
	packet.flags = (4 << 3) | 3; // Version number: 4, Mode: 3 (Client)

	if (sendto(s, (const char*)&packet, sizeof(packet), 0, (struct sockaddr*)&a, sizeof(a)) == SOCKET_ERROR) {
		sprintf(message, "Unable to send NTP request packet: %i", WSAGetLastError());
		MessageBox(0, message, APP_NAME, MB_OK | MB_ICONEXCLAMATION);
		WSACleanup();

		return FALSE;
	}

	if (recvfrom(s, (char *)&packet, sizeof(packet), 0, NULL, NULL) == SOCKET_ERROR) {
		sprintf(message, "Unable to receive NTP response packet: %i", WSAGetLastError());
		MessageBox(0, message, APP_NAME, MB_OK | MB_ICONEXCLAMATION);
		WSACleanup();

		return FALSE;
	}


	SYSTEMTIME st;
	if(!mapNtpTimeToSystemTime(ntohl(packet.transmitTimestamp), &st)) {
		MessageBox(0, "Unable to convert timestamp.", APP_NAME, MB_OK | MB_ICONEXCLAMATION);
		WSACleanup();

		return FALSE;
	}

	if (!SetSystemTime(&st)) {
		MessageBox(0, "Unable to set system time.", APP_NAME, MB_OK | MB_ICONEXCLAMATION);
		WSACleanup();

		return FALSE;
	}

	sprintf(
		message,
		"System clock has been set to %04i-%02i-%02i %02i:%02i:%02i UTC.",
		st.wYear,
		st.wMonth,
		st.wDay,
		st.wHour,
		st.wMinute,
		st.wSecond
	);
	MessageBox(0, message, APP_NAME, MB_OK | MB_ICONINFORMATION);

	WSACleanup();

	return TRUE;
}
