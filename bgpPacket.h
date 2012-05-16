/*
 * bpgPacket.h
 *
 *  Created on: May 18, 2011
 *      Author: schuch
 */

#ifndef BGPPACKET_H_
#define BGPPACKET_H_

typedef struct bgp_packet_t {
	void * data;
	ushort size;
} bgp_packet;

void freePacket(bgp_packet *deadPacket);
bgp_packet * buildBGPOpen(ushort myASN, int myIP);
bgp_packet * buildBGPKeepAlive();
bgp_packet * buildBGPUpdate(void *pathData, ushort pathSize, void *commData, ushort commSize, void * dest, int destSize, int myIP);

#endif /* BGPPACKET_H_ */
