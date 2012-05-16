/*
 * pathBuilder.c
 *
 *  Created on: May 19, 2011
 *      Author: schuch
 */
//TODO this code could be cleaned w/ the help of GLib

#include "pathBuilder.h"

/**
 * String helper function that splits a string into two pieces base on a delimiter.
 * The string will be split such that lhs = all characters up to the first occurrence of
 * the delimiter (BUT NOT THE DELIMETER ITSELF) and rhs = all characters to the right of
 * the first occurrence of the delimiter.  If the delimiter is not found, lhs and rhs
 * are not populated.
 *
 * 		orig - the original string
 * 		lhs - an allocated chunk of memory to store the left portion
 * 		rhs - an allocated chunk of memory to store the right portion
 * 		delim - the delimeter we're spliting the string on
 *
 * 		returns 1 if the delim is found, 0 otherwise
 */
int splitStr(char *orig, char *lhs, char *rhs, char delim) {
	int len = strlen(orig);
	int counter;

	for (counter = 0; counter < len; counter++) {
		if (orig[counter] == delim) {
			strcpy(rhs, orig + counter + 1);
			strncpy(lhs, orig, counter);
			lhs[counter] = 0x00;
			return 1;
		}
	}
	return 0;
}

/**
 *  This is a sane, re-entrant version of strtok.  It works a little bit different,
 *  it takes the string we're tokenizing, plus a pointer to allocated memory, finds
 *  the next token based on the delimeter that occures AFTER the startPos, and tells
 *  you the value startPos should be next time if you wanted to strtok like behavior.
 *  This will return the last token (i.e. the one without any delim after it).
 *
 *  		orig - the string we're tokenizing, this string will NOT be modified
 *  		nextToken - pointer to allocated memory where we can store the next token
 *  		delim - the delim we're tokenizing on
 *  		startPos - the position we should start the process from, to correctly
 *  				tokenize the string, the first call this should be 0, after that
 *  				it should be the previous call to saneTokenizer's return value
 *
 *  		returns  the next position to provide to startPos for the next call,
 *  				if we're at the end of the string (i.e. the last token was gathered)
 *  				then the strlen is returned, if an error occures, -1 is returned
 */
int saneTokenizer(char *orig, char *nextToken, char delim, int startPos) {
	int len = strlen(orig);
	int counter = 0;

	if (startPos >= len) {
		nextToken[0] = 0x00;
		return -1;
	}

	while (counter + startPos < len) {
		if (orig[counter + startPos] == delim) {
			strncpy(nextToken, orig + startPos, counter);
			nextToken[counter] = 0x00;
			return counter + startPos + 1;
		}
		counter++;
	}

	strcpy(nextToken, orig + startPos);
	return len;
}

/**
 * FIXME update comments
 *
 * Does initial packing of packet memory for a single AS segment path.  This
 * does all header setup, sets the size correctly (using extended size if needed)
 * and then populates your ASN as the first in the path, since that is required.
 *
 * 		pathMemBlock - allocated memory to hold this path data
 * 		size - the number of ASes in the path COUNTING MY OWN ASN
 * 		myASN - my own ASN
 *
 * 		returns the next byte in the memory path block that should be written to
 */
int startMultiSegmentPath(void *pathMemBlock, int asCount, int segmentCount) {
	int memEst = (asCount + segmentCount) * 2;

	/*
	 * Flags (WK, trans, comp)
	 * The fork is for if we need an extended length or not (4th bit)
	 * type to 2 (path)
	 */
	memset(pathMemBlock + 1, 0x02, 1);

	if (memEst > 255) {
		memset(pathMemBlock, 0x50, 1);
		memset(pathMemBlock + 2, (char) (memEst >> 8), 1);
		memset(pathMemBlock + 3, (char) (memEst), 1);
		return 4;
	} else {
		memset(pathMemBlock, 0x40, 1);
		memset(pathMemBlock + 2, (char) memEst, 1);
		return 3;
	}
}

/*
 * Does some amount of universal setup for a path
 * SIZE = SIZE WITHOUT MY OWN ASN
 */
/**
 * Does initial packing of packet memory for a single AS segment path.  This
 * does all header setup, sets the size correctly (using extended size if needed)
 * and then populates your ASN as the first in the path, since that is required.
 *
 * 		pathMemBlock - allocated memory to hold this path data
 * 		size - the number of ASes in the path NOT COUNTING MY OWN ASN
 * 		myASN - my own ASN
 *
 * 		returns the next byte in the memory path block that should be written to
 */
int startSingleSegmentPath(void *pathMemBlock, int size, ushort myASN) {
	int currentOffset = 0;

	/*
	 * Flags (WK, trans, comp)
	 * The fork is for if we need an extended length or not (4th bit)
	 * type to 2 (path)
	 */
	if (size <= 126) {
		memset(pathMemBlock, 0x40, 1);
	} else {
		memset(pathMemBlock, 0x50, 1);
	}
	memset(pathMemBlock + 1, 0x02, 1);

	/*
	 * size of path
	 */
	int byteSize = 2 + 2 * (size + 1);
	memset(pathMemBlock + 2, (char) byteSize, 1);
	currentOffset = 3;
	if (size > 126) {
		memset(pathMemBlock + 2, (char) (byteSize >> 8), 1);
		memset(pathMemBlock + 3, (char) (byteSize), 1);
		currentOffset = 4;
	} else {
		memset(pathMemBlock + 2, (char) (byteSize), 1);
	}

	/*
	 * Set that we're a segment, and how long we are
	 */
	memset(pathMemBlock + currentOffset, 0x02, 1);
	memset(pathMemBlock + currentOffset + 1, (char) (size + 1), 1);

	ushort netASN = htons(myASN);
	memcpy(pathMemBlock + currentOffset + 2, &netASN, 2);

	return currentOffset + 4;
}

/**
 * Simple packing method that creates a region of memory with monotonically
 * increasing ASNs in network order, will be of the form:
 * <startASN> <startASN + 1> <startASN + 2> ... <startASN + size - 1>
 *
 * 		memBlock - memory position to start writing
 * 		size - the number of ASNs written to memory
 * 		startASN - the ASN we start at
 */
void buildIncreasingPathSegment(void *memBlock, int size, int startASN) {
	int counter;
	ushort asn;
	for (counter = 0; counter < size; counter++) {
		asn = htons(counter + 50);
		memcpy(memBlock + (counter * 2), &asn, 2);
	}
}

int buildUniqueSegment(void *memBlock, int size, int startASN, path_counter *uniq, char type) {

	/*
	 * Build the base path once, as doing it more often is for suckers
	 */
	if (uniq->basePath == NULL) {
		/*
		 * Sanity check on path length, do this once, as the size should not change
		 */
		if (size > 255) {
			printf("asked to build a path that is too long\n");
			return 0;
		}

		/*
		 * grab some mem
		 */
		uniq->basePath = malloc(4096);
		if (uniq->basePath == NULL) {
			printf("error creating mem segment for unique path\n");
			return 0;
		}

		/*
		 * build the part that will be the same for all paths (headers plus the ASNs
		 * that form the padding), store the size
		 */

		memset(uniq->basePath, (char) size, 1);
		ushort netMyASN = htons(startASN);
		memcpy(uniq->basePath + 1, &netMyASN, 2);
		buildIncreasingPathSegment(uniq->basePath + 3, size - 3, 50);
		uniq->baseSize = 3 + ((size - 3) * 2);
	}

	/*
	 * place the base path into the memblock, then tack on
	 * the counter ASNs
	 */
	memset(memBlock, type, 1);
	memcpy(memBlock + 1, uniq->basePath, uniq->baseSize);
	ushort asn = htons(uniq->a);
	memcpy(memBlock + uniq->baseSize + 1, &asn, 2);
	asn = htons(uniq->b);
	memcpy(memBlock + uniq->baseSize + 3, &asn, 2);

	/*
	 * Move the counters
	 */
	uniq->a++;
	if (uniq->a == 2000) {
		uniq->a = 1000;
		uniq->b++;
	}

	return uniq->baseSize + 5;
}

ushort buildCommSegment(void *memBlock, int size, comm_counter *unique){

	/*
	 * short circuit to kick out asap if we're not actually doing comm attrs
	 */
	if(size == 0){
		return 0;
	}

	int memEst = size * 4;
	int offset = 0;
	memset(memBlock + 1, 0x08, 1);

	if (memEst > 255) {
		memset(memBlock, 0xd0, 1);
		memset(memBlock + 2, (char) (memEst >> 8), 1);
		memset(memBlock + 3, (char) (memEst), 1);
		offset = 4;
	} else {
		memset(memBlock, 0xc0, 1);
		memset(memBlock + 2, (char) memEst, 1);
		offset = 3;
	}

	int counter;
	int tempA, tempB;
	for(counter = 0; counter < size; counter++){
		tempA = htons(unique->a);
		tempB = htons(unique->b);
		memcpy(memBlock + offset + 4 * counter, &tempA, 2);
		memcpy(memBlock + offset + 2 + 4 * counter, &tempB, 2);

		unique->b++;
		if(unique->b > 60000){
			unique->b = 10000;
			unique->a++;
		}
	}

	return offset + size * 4;
}

ushort buildExtendedCommSegment(void *memBlock, int size, comm_counter *unique){

	/*
	 * Exit fast if we're not doing this
	 */
	if(size == 0){
		return 0;
	}

	int memEst = size * 8;
	int offset = 0;

	/*
	 * Setup headers (we're type 16)
	 */
	memset(memBlock + 1, 0x10, 1);
	if (memEst > 255) {
		memset(memBlock, 0xd0, 1);
		memset(memBlock + 2, (char) (memEst >> 8), 1);
		memset(memBlock + 3, (char) (memEst), 1);
		offset = 4;
	} else {
		memset(memBlock, 0xc0, 1);
		memset(memBlock + 2, (char) memEst, 1);
		offset = 3;
	}

	/*
	 * Build the damn things
	 */
	int counter;
	int tempA, tempB;
	for(counter = 0; counter < size; counter++){
		void *loopPtr = memBlock + offset + 8 * counter;
		tempA = htons(unique->a);
		tempB = htons(unique->b);
		memset(loopPtr, 0x40, 1);
		memset(loopPtr + 1, 0x03, 1);
		memset(loopPtr + 2, 0x01, 2);
		memcpy(loopPtr + 4, &tempA, 2);
		memcpy(loopPtr + 6, &tempB, 2);

		unique->b++;
		if(unique->b > 50000){
			unique->b = 10000;
			unique->a++;
		}
	}

	return offset + size * 8;
}

/**
 * Builds an AS path that should be a unique member of a set of paths.  The size should
 * be the TOTAL size of the path.  The path will be of the form:
 * <my asn> 50 51 52 .... (50 + size - 3) <a> <b>
 * where a and b are counters that ensure a unique path
 *
 * 		memBlock - allocated memory to store the path
 * 		size - the total size of the path including all ASNs
 * 		myASN - my own ASN, will be placed at the front of the path
 * 		counters - path_counter struct to ensure we generate a unique as path
 *
 * 		returns - the size of the path in memory
 */
ushort buildUniquePath(void *memBlock, int size, int commSize, int eCommSize, ushort myASN, path_counter *counters, comm_counter *commMachine) {

	/*
	 * Build the base path once, as doing it more often is for suckers
	 */
	if (counters->basePath == NULL) {
		/*
		 * Sanity check on path length, do this once, as the size should not change
		 */
		if (size > 255) {
			printf("asked to build a path that is too long\n");
			return 0;
		}

		/*
		 * grab some mem
		 */
		counters->basePath = malloc(4096);
		if (counters->basePath == NULL) {
			printf("error creating mem segment for unique path\n");
			return 0;
		}

		/*
		 * build the part that will be the same for all paths (headers plus the ASNs
		 * that form the padding), store the size
		 */
		int offset = startSingleSegmentPath(counters->basePath, size - 1, myASN);
		buildIncreasingPathSegment(counters->basePath + offset, size - 3, 50);
		counters->baseSize = offset + ((size - 3) * 2);
	}

	/*
	 * place the base path into the memblock, then tack on
	 * the counter ASNs
	 */
	memcpy(memBlock, counters->basePath, counters->baseSize);
	ushort asn = htons(counters->a);
	memcpy(memBlock + counters->baseSize, &asn, 2);
	asn = htons(counters->b);
	memcpy(memBlock + counters->baseSize + 2, &asn, 2);

	/*
	 * Move the counters
	 */
	counters->a++;
	if (counters->a == 2000) {
		counters->a = 1000;
		counters->b++;
	}

	int addOffset = buildCommSegment(memBlock + counters->baseSize + 4, commSize, commMachine);
	int eCommOffset = buildExtendedCommSegment(memBlock + counters->baseSize + 4 + addOffset, eCommSize, commMachine);

	return counters->baseSize + 4 + addOffset + eCommOffset;
}

/**
 * Builds an as path that will contain the given asn, plus monotonically
 * increasing ASNs (starting from 50), it's TOTAL length will be equal to
 * the given size.
 *
 * 		pathMemBlock - allocated memory to store the path
 * 		size - total size in ASNs of the path
 * 		myASN - the asn that should be first in the path
 *
 * 		Returns - the size of the actually written memory
 */
ushort buildIdentPath(void *pathMemBlock, int size, int commSize, ushort myASN, comm_counter *commMachine) {

	/*
	 * Sanity check on path length
	 */
	if (size > 255) {
		printf("asked to build a path that is too long\n");
		return 0;
	}

	/*
	 * Build the path headers, remeber, size supplied to startSingleSegment
	 * is the size without our ASN pre-pended
	 */
	int offset = startSingleSegmentPath(pathMemBlock, size - 1, myASN);

	/*
	 * Drop in the rest of the path
	 */
	buildIncreasingPathSegment(pathMemBlock + offset, size - 1, 50);

	/*
	 * Size is everything given by startSingleSegmentPath (headers + our asn) and the
	 * monotonically increasing ASNs
	 */
	return offset + ((size - 1) * 2);
}

asn_seg_list *parseOneSegment(char *asToken, char type) {

	/*
	 * Parse out the asn path
	 */
	int asSize = 0;
	asn_list *asPathHead = NULL;
	asn_list *asPathTail = NULL;
	char *subToken = calloc(6, 1);
	int pathTokenPos = 0;
	if(type == 0x02){
	  pathTokenPos = saneTokenizer(asToken, subToken, ' ', 0);
	}
	else{
	  pathTokenPos = saneTokenizer(asToken, subToken, ',', 0);
	}
	while (strlen(subToken) > 0) {
		asn_list *newPtr = malloc(sizeof(asn_list));
		int tempASN = atoi(subToken);

		/*
		 * Do tail insertion, otherwise we get a backward asn list
		 */
		if (asPathHead == NULL) {
			asPathHead = newPtr;
			asPathTail = newPtr;
		} else {
			asPathTail->next = newPtr;
			asPathTail = newPtr;
		}

		newPtr->next = NULL;
		newPtr->asn = tempASN;
		asSize++;
		if(type == 0x02){
		  pathTokenPos = saneTokenizer(asToken, subToken, ' ', pathTokenPos);
		}
		else{
		  pathTokenPos = saneTokenizer(asToken, subToken, ',', pathTokenPos);
		}
	}

	/*
	 * Memory cleanup
	 */
	free(subToken);

	/*
	 * Build our seg list element
	 */
	asn_seg_list *retSeg = malloc(sizeof(asn_seg_list));
	retSeg->data = asPathHead;
	retSeg->type = type;
	retSeg->size = asSize;
	retSeg->next = NULL;

	return retSeg;
}

//TODO parse comm attrs out of the file as well
size_pair buildFilePath(void *pathMemBlock, void *nlriMemBlock, ushort myASN, char *configString) {

	/*
	 * Grabs proto version
	 */
	char *tokens = strtok(configString, "|");
	/*
	 * Grab time stamp (throw away)
	 */
	tokens = strtok(NULL, "|");
	/*
	 * Grab advertise flag (throw away)
	 */
	tokens = strtok(NULL, "|");
	/*
	 * Next hop IP Address (throw away)
	 */
	tokens = strtok(NULL, "|");
	/*
	 * Local Pref?/Next ASN?? (throw away)
	 */
	tokens = strtok(NULL, "|");
	/*
	 * NLRI
	 */
	char *nlriToken = strtok(NULL, "|");
	/*
	 * AS Path
	 */
	char *asToken = strtok(NULL, "|");
	/*
	 * Currently the stuff after this doesn't really matter, but there is other stuff in the line after this
	 */

	char *firstASTest = calloc(6, 1);
	saneTokenizer(asToken, firstASTest, ' ', 0);
	ushort testVal = atoi(firstASTest);
	if(testVal != myASN){
		char *holder = calloc(strlen(asToken) + 6, 1);
		sprintf(holder, "%u %s", myASN, asToken);
		asToken = holder;
	}
	free(firstASTest);

	int segmentCount = 0;
	int totalASSize = 0;
	asn_seg_list *asSegHead = NULL;
	asn_seg_list *asSegTail = NULL;
	asn_seg_list *oldSeg = NULL;
	char currentType = 0x02;

	/*
	 * Tease out each sequence or set
	 */
	char *tempSeg = calloc(4096, 1);
	int segParsePos = saneTokenizer(asToken, tempSeg, '{', 0);
	while (strlen(tempSeg) != 0) {
		/*
		 * Do the parse, record size stats
		 */
		asn_seg_list *newSeg = parseOneSegment(tempSeg, currentType);
		segmentCount++;
		totalASSize += newSeg->size;

		/*
		 * Update pointers
		 */
		if (asSegHead == NULL) {
			asSegHead = newSeg;
		}
		if (asSegTail != NULL) {
			asSegTail->next = newSeg;
		}
		asSegTail = newSeg;

		/*
		 * grab the next chunk of the string
		 */
		if (currentType == 0x02) {
			segParsePos = saneTokenizer(asToken, tempSeg, '}', segParsePos);
			currentType = 0x01;
		} else {
			segParsePos = saneTokenizer(asToken, tempSeg, '{', segParsePos);
			currentType = 0x02;
		}
	}
	free(tempSeg);

	/*
	 * Build the headers, then fill in the configured path
	 * NOTE: the size is + 1, since it's our ASN plus the asn path in the file
	 */
	int offset = startMultiSegmentPath(pathMemBlock, totalASSize, segmentCount);
	while (asSegHead != NULL) {
		/*
		 * Add the segment headers
		 */
	  char tempType = asSegHead->type;
	  int tempSize = asSegHead->size;
	  memcpy(pathMemBlock + offset, &tempType, 1);
	  memcpy(pathMemBlock + offset + 1, &tempSize, 1);

		/*
		 * Actually pack the ASes
		 */
		asn_list *asPathHead = asSegHead->data;
		asn_list *oldPtr = NULL;
		int posCounter = 0;
		while (asPathHead != NULL) {
			ushort tempASN = htons(asPathHead->asn);
			memcpy(pathMemBlock + offset + 2 + (posCounter * 2), &tempASN, 2);
			oldPtr = asPathHead;
			asPathHead = asPathHead->next;
			free(oldPtr);
			posCounter++;
		}

		/*
		 * Update pointers and offsets
		 */
		offset += posCounter * 2 + 2;
		oldSeg = asSegHead;
		asSegHead = asSegHead->next;
		free(oldSeg);
	}

	/*
	 * Parse out the nlri
	 */
	char *netBits = calloc(16, 1);
	char *netSizeStr = calloc(3, 1);
	splitStr(nlriToken, netBits, netSizeStr, '/');
	int netSizeInt = atoi(netSizeStr);
	memcpy(nlriMemBlock, &netSizeInt, 1);

	char *octet = calloc(4, 1);
	int saneTokenPos = saneTokenizer(netBits, octet, '.', 0);
	int occtetCounter = 0;
	while (netSizeInt > 0) {
		int currentOctetInt = atoi(octet);
		memcpy(nlriMemBlock + 1 + occtetCounter, &currentOctetInt, 1);
		saneTokenPos = saneTokenizer(netBits, octet, '.', saneTokenPos);
		netSizeInt -= 8;
		occtetCounter++;
	}
	free(octet);
	free(netBits);
	free(netSizeStr);

	size_pair retValue;
	retValue.pathSize = offset;
	retValue.memSize = 1 + occtetCounter;
	return retValue;
}

ushort buildFracturedPath(void *pathMemBlock, int blockSize, int blockCount, int commSize, path_counter *uniqueness, comm_counter *commMachine, ushort myASN) {

	int offset = startMultiSegmentPath(pathMemBlock, blockCount * blockSize, blockCount);

	int packedCounter = 0;
	for (packedCounter = 0; packedCounter < blockCount; packedCounter++) {
		offset += buildUniqueSegment(pathMemBlock + offset, blockSize, myASN, uniqueness, (char) ((packedCounter + 1)
				% 2 + 1));
	}

	offset += buildCommSegment(pathMemBlock + offset, commSize, commMachine);

	return offset;
}

ushort buildSlidingFracturedPath(void *pathMemBlock, int baseSize, int setSize, path_counter *frontUniq, path_counter *backUniq, ushort myASN){

	int offset = startMultiSegmentPath(pathMemBlock, baseSize, 2);

	offset += buildUniqueSegment(pathMemBlock + offset, baseSize - setSize, myASN, frontUniq, 0x02);
	offset += buildUniqueSegment(pathMemBlock + offset, setSize, myASN, backUniq, 0x01);

	return offset;
}

ushort buildTestPath(void *pathMemBlock) {

	/*
	 * Test headers
	 * Flags (WK, trans, comp, not ext)
	 * set type to 2 (path)
	 * set size to 6 (type + size + 2x ases)
	 */
	memset(pathMemBlock, 0x40, 1);
	memset(pathMemBlock + 1, 0x02, 1);
	memset(pathMemBlock + 2, 0x06, 1);

	/*
	 * Set the fact that it is a segment, and how long it is
	 */
	memset(pathMemBlock + 3, 0x02, 1);
	memset(pathMemBlock + 4, 0x02, 1);

	ushort asn = htons(4);
	memcpy(pathMemBlock + 5, &asn, 2);
	asn = htons(16);
	memcpy(pathMemBlock + 7, &asn, 2);

	return 9;
}

