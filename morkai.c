/*
 * morkai.c
 *
 *  Created on: May 18, 2011
 *      Author: schuch
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/types.h>
#include "bgpPacket.h"
#include "pathBuilder.h"

#define BGP_DEFAULT_PORT 179

enum MODE {
	samePath,
	uniquePath,
	fileConfig,
	fracConfig,
	slidingFracConfig,
	blackhole
} myMode;

/*
 * Error codes
 * 0 - success
 * 1 - IO/Mem error
 * 3 - bgp error
 * 4 - config error
 */

/*
 * Usage: morkai <peer IP> <my IP> <my ASN> <mode>
 */
/*
 * TODO testing
 * 0) compile
 * 1) check that synthetic nlri are still sane
 * 2) check that pushing 100k synthetic routes works
 * 3) test config file push w/ speed check & mem check
 */
//FIXME there is a mem leak here when dealing w/ config files, might want to fix it, might not actually give a shit
int main(int argc, char *argv[]) {

	if (argc < 6) {
		printf(
				"usage: morkai <peer IP> <salt> <my IP> <my ASN> <mode> <mode spec configs....>\n");
		return 4;
	}

	int salt = atoi(argv[2]);
	int myASN = atoi(argv[4]);
	FILE *configFile;

	/*
	 * Parse out the mode and ASN
	 */
	if (strcmp(argv[5], "s") == 0) {
		if (argc != 9) {
			printf(
					"invalid same path config - usage: s <path size> <comm size> <number of paths>\n");
			return 4;
		}
		myMode = samePath;
	} else if (strcmp(argv[5], "u") == 0) {
		if (argc != 10) {
			printf(
					"invalid same path config - usage: u <path size> <comm size> <extended comm size> <number of paths>\n");
			return 4;
		}
		myMode = uniquePath;
	} else if (strcmp(argv[5], "f") == 0) {
		if (argc != 7) {
			printf("invalid file config - usage: f <config path>\n");
			return 4;
		}
		myMode = fileConfig;

		/*
		 * Open file, check for errors
		 */
		configFile = fopen(argv[6], "r");
		if (configFile == NULL) {
			printf("error opening config file: %s\n", strerror(errno));
			return 1;
		}
	} else if (strcmp(argv[5], "c") == 0) {
		myMode = fracConfig;
		if (argc != 10) {
			printf(
					"invalid fractured config - usage: c <block size> <block count> <comm size> <number of paths>\n");
			return 1;
		}
	} else if (strcmp(argv[5], "l") == 0) {
		myMode = slidingFracConfig;
		if (argc != 9) {
			printf(
					"invalid sliding fracture config - usage: l <total size> <set size> <number of paths>\n");
			return 1;
		}
	} else if (strcmp(argv[5], "b") == 0) {
		myMode = blackhole;
	} else {
		printf(
				"unknown mode - valid options are (s)ame path, (u)nique path, (f)ile, (b)lack hole, fra(c)tured, and s(l)iding\n");
		return 4;
	}

	/*
	 * Some logging plus some time book keeping.
	 */
	printf("starting up\n");
	fflush(stdout);
	struct timeval time;
	gettimeofday(&time, NULL);
	double t1 = time.tv_sec + (time.tv_usec / 1000000.0);

	/*
	 * Build the struct holding our peer's information
	 */
	struct sockaddr_in bgpPeerAddr;
	bgpPeerAddr.sin_family = AF_INET;
	bgpPeerAddr.sin_port = htons(BGP_DEFAULT_PORT);
	if (inet_aton(argv[1], &bgpPeerAddr.sin_addr) == 0) {
		printf("peer address isn't valid\n");
		return 4;
	}

	/*
	 * Build the socket and connect it
	 */
	int bgpSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (connect(bgpSocket, (struct sockaddr *) &bgpPeerAddr,
			sizeof(bgpPeerAddr))) {
		printf("error binding socket to bgp peer\n");
		printf("%s\n", strerror(errno));
		return 1;
	}
	printf("done building socket and connecting\n");
	fflush(stdout);

	/*
	 * Build the open packet & sends it
	 */
	struct in_addr myIP;
	inet_aton(argv[3], &myIP);
	bgp_packet * openPacket = buildBGPOpen(myASN, myIP.s_addr);
	if (openPacket == NULL) {
		printf("failed to build open, dying\n");
		close(bgpSocket);
		return 1;
	}
	write(bgpSocket, openPacket -> data, openPacket -> size);
	freePacket(openPacket);
	printf("done sending open, waiting\n");

	/*
	 * Grab the open response, read all of it to clear out the network buffer,
	 * and move on, we really don't care about the contents, as we're not
	 * actually a bgp speaker
	 */
	char incomingBuff[4096];
	read(bgpSocket, incomingBuff, 19);
	if (incomingBuff[18] != 0x01) {
		printf("didn't get open message, killing myself\n");
		return 3;
	}
	int toRead = (int) incomingBuff[17];
	read(bgpSocket, incomingBuff, toRead - 19);
	printf("done getting open\n");
	fflush(stdout);

	/*
	 * Build the opening keep alive and send it
	 */
	bgp_packet * keepAlivePacket = buildBGPKeepAlive();
	if (keepAlivePacket == NULL) {
		printf("failed to build keep alive, dying\n");
		close(bgpSocket);
		return 1;
	}
	write(bgpSocket, keepAlivePacket -> data, keepAlivePacket -> size);
	freePacket(keepAlivePacket);
	printf("done sending keep alive\n");

	/*
	 * Get the keep alive, or at least make sure the other side sent, at this point we can start
	 * ignoring the other side of this connection
	 */
	read(bgpSocket, incomingBuff, 19);
	if (incomingBuff[18] != 0x04) {
		printf("didn't get keep alive, killing myself....\n");
		return 3;
	}
	printf("connection fully opened, good to go\n");
	fflush(stdout);

	/*
	 * Small amount of setup before we start advs
	 */
	char *destMem = malloc(5);
	int prefixTail = 0;
	int numberOfPaths = 0;
	if (myMode == samePath || myMode == uniquePath || myMode == fracConfig
			|| myMode == slidingFracConfig) {
		if (myMode == fracConfig || myMode == uniquePath) {
			numberOfPaths = atoi(argv[9]);
		} else {
			numberOfPaths = atoi(argv[8]);
		}
		destMem[0] = 0x18;
		destMem[1] = 0x80;
	}

	path_counter *pathMachine;
	path_counter *supPathMachine;
	comm_counter *commMachine;
	mem_list *asMemList;
	mem_list *nlriMemList;
	/*
	 * XXX in theory this should be fine, if my 5271 students could see
	 * this they would shit bricks of course...
	 */
	char readString[5000];

	/*
	 * If we're doing a same path run, generate the path once,
	 * else if we're doing unique paths, malloc the space for the machinery to make that work
	 */
	void *pathMem = malloc(4096);
	int asCount, blockCount, commCount, eCommCount;
	ushort pathSize;
	int nlriSize = 4;

	/*
	 * setup comm attribute machine
	 */
	if (myMode == samePath || myMode == uniquePath || myMode == fracConfig) {
		commMachine = malloc(sizeof(comm_counter));
		commMachine->a = 1000 + (salt * 100);
		commMachine->b = 10000;
	}

	if (myMode == samePath) {
		pathSize = buildIdentPath(pathMem, atoi(argv[6]), atoi(argv[7]), myASN,
				commMachine);

		if (pathSize == 0) {
			printf("error building path!\n");
			return 2;
		}
	} else if (myMode == uniquePath) {
		pathMachine = malloc(sizeof(path_counter));
		if (pathMachine == NULL) {
			printf("error mallocing path machine for unique paths\n");
			return 1;
		}
		pathMachine -> a = 1000;
		pathMachine -> b = 10000;
		pathMachine -> basePath = NULL;
		pathMachine -> baseSize = 0;
		asCount = atoi(argv[6]);
		commCount = atoi(argv[7]);
		eCommCount = atoi(argv[8]);
	} else if (myMode == fileConfig) {
		while (!feof(configFile)) {
			mem_list *pathNewPtr = malloc(sizeof(mem_list));
			mem_list *nlriNewPtr = malloc(sizeof(mem_list));
			pathNewPtr->mem = malloc(1024);
			nlriNewPtr->mem = malloc(5);
			memset(readString, 0x00, 5000);
			fgets(readString, 5000, configFile);
			if (strlen(readString) == 0) {
				continue;
			}
			size_pair sizes = buildFilePath(pathNewPtr->mem, nlriNewPtr->mem,
					myASN, readString);
			pathNewPtr->size = sizes.pathSize;
			nlriNewPtr->size = sizes.memSize;
			pathNewPtr->next = asMemList;
			nlriNewPtr->next = nlriMemList;
			asMemList = pathNewPtr;
			nlriMemList = nlriNewPtr;
			numberOfPaths++;
		}
	} else if (myMode == fracConfig) {
		pathMachine = malloc(sizeof(path_counter));
		if (pathMachine == NULL) {
			printf("error mallocing path machine for unique paths\n");
			return 1;
		}
		pathMachine -> a = 1000;
		pathMachine -> b = 10000;
		pathMachine -> basePath = NULL;
		pathMachine -> baseSize = 0;
		asCount = atoi(argv[6]);
		blockCount = atoi(argv[7]);
		commCount = atoi(argv[8]);
	} else if (myMode == slidingFracConfig) {
		pathMachine = malloc(sizeof(path_counter));
		supPathMachine = malloc(sizeof(path_counter));
		if (pathMachine == NULL || supPathMachine == NULL) {
			printf("error mallocing path machine for unique paths\n");
			return 1;
		}
		pathMachine -> a = 1000;
		supPathMachine -> a = 1000;
		pathMachine -> b = 10000;
		supPathMachine -> b = 10000;
		pathMachine -> basePath = NULL;
		supPathMachine -> basePath = NULL;
		pathMachine -> baseSize = 0;
		supPathMachine -> baseSize = 0;
		asCount = atoi(argv[6]);
		blockCount = atoi(argv[7]);
	}

	/*
	 * Loop that generates the updates and sends them
	 */
	for (prefixTail = 0; prefixTail < numberOfPaths; prefixTail++) {

		/*
		 * If we're not building from a file, simply get the next nlri in the line
		 */
		if (myMode != fileConfig) {
		        destMem[1] = (char) (prefixTail >> 16) | (0x80 + salt);
			destMem[2] = (char) (prefixTail >> 8);
			destMem[3] = (char) prefixTail;
		}

		/*
		 * Build unique path each time we go to adv
		 */
		if (myMode == uniquePath) {
			pathSize = buildUniquePath(pathMem, asCount, commCount, eCommCount, myASN,
					pathMachine, commMachine);
			if (pathSize == 0) {
				printf("error building path!\n");
				return 2;
			}
		} else if (myMode == fileConfig) {
			pathMem = asMemList->mem;
			destMem = nlriMemList->mem;
			pathSize = asMemList->size;
			nlriSize = nlriMemList->size;

			/*
			 * Update Pointers to linked lists
			 */
			asMemList = asMemList->next;
			nlriMemList = nlriMemList->next;
		} else if (myMode == fracConfig) {
			pathSize = buildFracturedPath(pathMem, asCount, blockCount,
					commCount, pathMachine, commMachine, myASN);
		} else if (myMode == slidingFracConfig) {
			pathSize = buildSlidingFracturedPath(pathMem, asCount, blockCount,
					pathMachine, supPathMachine, myASN);
		}

		bgp_packet *updatePacket = buildBGPUpdate(pathMem, pathSize, NULL, 0,
				(void *) destMem, nlriSize, myIP.s_addr);
		write(bgpSocket, updatePacket -> data, updatePacket -> size);
		freePacket(updatePacket);
	}
	printf("All updates sent!\n");
	fflush(stdout);

	/*
	 * A little bit more time keeping, log the runtime
	 */
	gettimeofday(&time, NULL);
	double t2 = time.tv_sec + (time.tv_usec / 1000000.0);
	printf("%.6lf seconds elapsed\n", t2 - t1);
	fflush(stdout);

	/*
	 * If we had a file open to read the config from, close it
	 */
	if (myMode == fileConfig) {
		fclose(configFile);
	}

	/*
	 * Sleep the process for 5 mins, should be enough time
	 */
	sleep(300);

	return 0;
}
