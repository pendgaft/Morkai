/*
 * bgpPacket.c
 *
 *  Created on: May 18, 2011
 *      Author: schuch
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>

#include "bgpPacket.h"

bgp_packet *startNewPacket(ushort payloadSize, char type) {
	/*
	 * Allocate the packet
	 */
	bgp_packet *retPacket = malloc(sizeof(bgp_packet));
	if (retPacket == NULL) {
		printf("Memory malloc issue building bgp packet\n");
		return NULL;
	}

	/*
	 * Sanity check on packet size
	 */
	retPacket->size = payloadSize + 19;
	if (retPacket->size > 4096) {
		printf("Asked to make too large of a packet\n");
		free(retPacket);
		return NULL;
	}

	/*
	 * Allocate the actual packet space
	 */
	retPacket->data = malloc(retPacket -> size);
	if (retPacket -> data == NULL) {
		printf("Memory malloc issue building bgp packet\n");
		return NULL;
	}

	//Set marker field
	memset(retPacket -> data, -1, 16);

	//Set length field
	short netSize = htons(retPacket -> size);
	memcpy(retPacket -> data + 16, &netSize, 2);

	//Set type field
	memset(retPacket -> data + 18, type, 1);

	return retPacket;
}

void freePacket(bgp_packet *deadPacket) {
	free(deadPacket -> data);
	free(deadPacket);
	return;
}

bgp_packet *buildBGPOpen(ushort myASN, int myIP) {

	bgp_packet * openPacket = startNewPacket(10, 0x01);
	if (openPacket == NULL) {
		printf("Error making open packet\n");
		return NULL;
	}

	short netASN = htons(myASN);

	//set version
	memset(openPacket -> data + 19, 0x04, 1);

	//set my asn
	memcpy(openPacket -> data + 20, &netASN, 2);

	//set to no hold timer
	memset(openPacket -> data + 22, 0, 2);

	//set my BGP identifier
	memcpy(openPacket -> data + 24, &myIP, 4);

	//set the size of optional params to 0
	memset(openPacket -> data + 28, 0x00, 1);

	return openPacket;
}

bgp_packet *buildBGPKeepAlive() {

	bgp_packet * kaPacket = startNewPacket(0, 0x04);
	if (kaPacket == NULL) {
		printf("error building keep alive\n");
		return NULL;
	}

	return kaPacket;
}

bgp_packet *buildBGPUpdate(void *pathData, ushort pathSize, void *commData, ushort commSize, void *dest, int destSize, int myIP) {

	/*
	 * Compute the payload size from the comm attrs, plus path, plus required stuff
	 *
	 * Reuired stuff:
	 *  withdrawl length 2
	 *  Attr length 2
	 *  Origin length 4
	 *  Next Hop length 7
	 */
	ushort totalSize = pathSize + commSize + destSize + 15;
	bgp_packet *builtPacket = startNewPacket(totalSize, 0x02);
	if (builtPacket == NULL) {
		printf("Error mallocing update packet\n");
		return NULL;
	}

	/*
	 * Sets the size of the withdrawl field to 0, as we never send them
	 */
	memset(builtPacket->data + 19, 0x00, 2);

	/*
	 * Sets the attribute length field, which is 8 less then then the total payload size
	 */
	ushort attrLengthTotal = htons(pathSize + commSize + 11);
	memcpy(builtPacket -> data + 21, &attrLengthTotal, 2);

	/*
	 * Set the origin attribute's
	 * flags: well know, trans, complete, not ext length
	 * set it's code, size, and value, which are all 0x01
	 */
	memset(builtPacket -> data + 23, 0x40, 1);
	memset(builtPacket -> data + 24, 0x01, 3);

	/*
	 * Set the next hop attribute
	 * flags: well known, trans, complete, not ext length
	 * set code to 3, size to 4, and then the hardwired 10.0.0.15
	 */
	//FIXME set to actually load the ip from config
	memset(builtPacket -> data + 27, 0x40, 1);
	memset(builtPacket -> data + 28, 0x03, 1);
	memset(builtPacket -> data + 29, 0x04, 1);
	memcpy(builtPacket -> data + 30, &myIP, 4);

	/*
	 * Pack the path data and comm data (if the later exists)
	 */
	memcpy(builtPacket -> data + 34, pathData, pathSize);
	if (commSize > 0) {
		memcpy(builtPacket -> data + 34 + pathSize, commData, commSize);
	}

	/*
	 * Packs the nlri of the update
	 */
	memcpy(builtPacket -> data + 34 + pathSize + commSize, dest, destSize);

	return builtPacket;
}

