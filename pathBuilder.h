/*
 * pathBuilder.h
 *
 *  Created on: May 19, 2011
 *      Author: schuch
 */

#ifndef PATHBUILDER_H_
#define PATHBUILDER_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>

typedef struct path_counter_t {
	int a;
	int b;
	void *basePath;
	int baseSize;
} path_counter;

typedef struct comm_counter_t {
	int a;
	int b;
} comm_counter;

typedef struct mem_list_t{
	void *mem;
	ushort size;
	struct mem_list_t *next;
}mem_list;

typedef struct asn_list_t{
	ushort asn;
	struct asn_list_t *next;
}asn_list;

typedef struct asn_seg_list_t{
	asn_list *data;
	int size;
	char type;
	struct asn_seg_list_t *next;
}asn_seg_list;

typedef struct size_pair_t{
	ushort pathSize;
	int memSize;
}size_pair;

ushort buildUniquePath(void *memBlock, int size, int commSize, int eCommSize, ushort myASN, path_counter *counters, comm_counter *commMachine);

/*
 * Generates a path of a given size.
 * This is intended for generating runs of synthetic updates with the same as path.
 * Will generate a path based on <my ASN>, 50, 51, .....
 */
ushort buildIdentPath(void *memBlock, int size, int commSize, ushort myASN, comm_counter *commMachine);

size_pair buildFilePath(void *asBlock, void *ipBlock, ushort myASN, char * configLine);

ushort buildFracturedPath(void *pathMemBlock, int blockSize, int blockCount, int commSize, path_counter *uniqueness, comm_counter *commMachine, ushort myASN);

ushort buildSlidingFracturedPath(void *pathMemBlock, int pathSize, int setSize, path_counter *frontUniq, path_counter *backUniq, ushort myASN);

/*
 * This was a test function, prob do not want to call it
 */
ushort buildTestPath(void *memBlock);

#endif /* PATHBUILDER_H_ */
