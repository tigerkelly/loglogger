
/* Program to receive data for logging to a file. */

#include <stdio.h>      // for printf() and fprintf()
#include <sys/socket.h> // for socket(), connect(), sendto(), and recvfrom()
#include <arpa/inet.h>  // for sockaddr_in and inet_addr()
#include <stdlib.h>     // for atoi() and exit()
#include <string.h>     // for memset()
#include <signal.h>
#include <netdb.h>
#include <unistd.h>     // for close()
#include <time.h>       // for time()
#include <sys/time.h>   // for settimeofday()
#include <math.h>       // for abs()
#include <ctype.h>

#define MILLION				1000000
#define KILOBYTE			1024
#define MAX_FILE_SIZE		(5 * MILLION)
#define BASE_PATH			"/home/pi/CommandMesh"
#define LOG_NAME			"commandmesh.log"
#define BACKUP_LOG_NAME1	"commandmesh.log.1"
#define BACKUP_LOG_NAME2	"commandmesh.log.2"

#define MAXRECVSTRING	1024		// Longest string to receive
#define MAX_LOGMSG		(1024 + 64)	// add extra for timestamp

FILE *out = NULL;
char filePath[1024];
char backupPath1[1024];
char backupPath2[1024];

void DieWithError(char *errorMessage);  // External error handling function
void archiveLog(void);
void handleSignal(int sig);
void handleArchive(int sig);

int allAscii(char *s );

int main(int argc, char *argv[])
{
	int sock;							// Socket
	struct sockaddr_in broadcastAddr;	// Broadcast Address
	unsigned short logloggerPort;		// Port
	char recvString[MAXRECVSTRING+1];	// Buffer for received string
	int recvStringLen;					// Length of received string

	if (signal(SIGINT, handleSignal) == SIG_ERR) {
		printf("Can't catch SIGINT\n");
    }

    if (signal(SIGTERM, handleSignal) == SIG_ERR) {
		printf("Can't catch SIGTERM\n");
    }

    if (signal(SIGUSR1, handleArchive) == SIG_ERR) {
		printf("Can't catch SIGUSR1\n");
    }

	if (argc < 2) {
		struct servent *sp = getservbyname("loglogger", "udp");
		if (sp == NULL) {
			printf("Can not get service loglogger.\n");
			exit(1);
		}
		logloggerPort = sp->s_port;
	} else
		logloggerPort = htons(atoi(argv[1]));   // First arg: broadcast listening port

	// Create a best-effort datagram socket using UDP
	if ((sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
		DieWithError("socket() failed");
	}

	// Construct bind structure
	memset(&broadcastAddr, 0, sizeof(broadcastAddr));   // Zero out structure
	broadcastAddr.sin_family = AF_INET;                 // Internet address family
	broadcastAddr.sin_addr.s_addr = htonl(INADDR_ANY);  // Any incoming interface
	broadcastAddr.sin_port = logloggerPort;      // Broadcast port

	// Bind to the broadcast port
	if (bind(sock, (struct sockaddr *) &broadcastAddr, sizeof(broadcastAddr)) < 0) {
		DieWithError("bind() failed");
	}

	char logMsg[MAX_LOGMSG];
	memset(filePath, 0, sizeof(filePath));
	memset(backupPath1, 0, sizeof(backupPath1));
	memset(backupPath2, 0, sizeof(backupPath2));
	memset(logMsg, 0, sizeof(logMsg));

	sprintf(filePath, "%s/%s", BASE_PATH, LOG_NAME);
	sprintf(backupPath1, "%s/%s", BASE_PATH, BACKUP_LOG_NAME1);
	sprintf(backupPath2, "%s/%s", BASE_PATH, BACKUP_LOG_NAME2);

	out = fopen(filePath, "a");	// append mode
	if (out == NULL) {
		DieWithError("fopen() failed");
	}

	struct tm *tm_info;
	struct timespec ts;
	char tbuf[32];
	char sbuf[32];

	clock_gettime(CLOCK_REALTIME, &ts);
	tm_info = localtime(&ts.tv_sec);

	strftime(tbuf, sizeof(tbuf), "%Y-%m-%d %H:%M:%S", tm_info);

	sprintf(sbuf, ".%03ld", (ts.tv_nsec / MILLION));

	strcat(tbuf, sbuf);

	sprintf(logMsg, "%s, *** LogLogger started ***\n", tbuf);

	if (allAscii(logMsg)) {
		fwrite(logMsg, 1, strlen(logMsg), out);
		fflush(out);
	}

	sprintf(logMsg, "%s, *** Compiled: %s ***\n", tbuf, __TIMESTAMP__);

	if (allAscii(logMsg)) {
		fwrite(logMsg, 1, strlen(logMsg), out);
		fflush(out);
	}

	for ( ;; ) {
		// Receive a single datagram from the server
		memset(recvString, 0, sizeof(recvString));
		if ((recvStringLen = recvfrom(sock, recvString, MAXRECVSTRING, 0, NULL, 0)) < 0) {
			DieWithError("recvfrom() failed");
		}

		recvString[recvStringLen] = '\0';

		char *p = strrchr(recvString, '\n');
		if (p != NULL)
			*p = '\0';

		// printf("recvString: %s\n", recvString);

		clock_gettime(CLOCK_REALTIME, &ts);
		tm_info = localtime(&ts.tv_sec);

		strftime(tbuf, sizeof(tbuf), "%Y-%m-%d %H:%M:%S", tm_info);

		sprintf(sbuf, ".%03ld", (ts.tv_nsec / MILLION));

		strcat(tbuf, sbuf);

		sprintf(logMsg, "%s, %s\n", tbuf, recvString);

		if (allAscii(logMsg)) {
			fwrite(logMsg, 1, strlen(logMsg), out);

			fflush(out);
		}

		size_t size = ftell(out);
		// fprintf(stderr, "size %d  %d\n", size, MAX_FILE_SIZE);
		if (size >= MAX_FILE_SIZE) {
			archiveLog();
		}
	}

	if (out != NULL)
		fclose(out);

	close(sock);
	exit(0);
}

int allAscii(char *s) {
	int ret = 1;
	int len = 0;

	if (s == NULL)
		return ret;
	
	len = strlen(s);

	for (int i = 0; i < len; i++, s++) {
		if (isascii(*s) == 0) {
			ret = 0;
			break;
		}
	}

	return ret;
}

void archiveLog() {
	fprintf(out, "Archive log, changing to new file.\n");
	fflush(out);
	fclose(out);

	if( access( backupPath2, F_OK ) != -1 ) {
		remove( backupPath2);
	}
	if( access(backupPath1, F_OK ) != -1 ) {
		rename(backupPath1, backupPath2);
	}

	if( access( backupPath1, F_OK ) != -1 ) {
		remove( backupPath1);
	}

	if( access(filePath, F_OK ) != -1 ) {
		rename(filePath, backupPath1);
	}

	out = fopen(filePath, "a");
	if (out == NULL) {
		DieWithError("2. fopen() failed");
	}
}

void DieWithError(char *errMsg) {
	fprintf(stderr, "%s\n", errMsg);
	exit(1);
}

void handleArchive(int sig) {

	archiveLog();
}

void handleSignal(int sig) {

    signal(SIGTERM, SIG_DFL);
    signal(SIGINT, SIG_DFL);

	if(out != NULL)
		fclose(out);

	exit(1);
}
