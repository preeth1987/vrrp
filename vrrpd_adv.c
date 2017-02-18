#include "header.h"
#include "vrrp.h"
#include "libvrrp.h"

unsigned char phy_dst[] = { 0x01, 0x00, 0x5e, 0x00, 0x00, 0x12 };

/*Create a raw socket */
int CreateSock(int proto){

	int sock;
	
	if((sock=socket(PF_PACKET,SOCK_RAW,htons(proto)))==-1)
	{
		perror("Creating socket failed\n");
		exit (-1);
	}
	return sock;
}

int BindSock(char *device, int raw, int protocol){
	
	struct sockaddr_ll s1;
	struct ifreq ifr;

	bzero(&s1, sizeof(s1));
	bzero(&ifr, sizeof(ifr));
	
	/* First Get the Interface Index  */


	strncpy((char *)ifr.ifr_name, device, IFNAMSIZ);
	if((ioctl(raw, SIOCGIFINDEX, &ifr)) == -1)
	{
		printf("Error getting Interface index !\n");
		exit(-1);
	}

	/* Bind our raw socket to this interface */

	s1.sll_family = AF_PACKET;
	s1.sll_ifindex = ifr.ifr_ifindex;
	s1.sll_protocol = htons(protocol); 


	if((bind(raw, (struct sockaddr *)&s1, sizeof(s1)))== -1)
	{
		perror("Error binding raw socket to interface\n");
		exit(-1);
	}

	return 1;
	
}



/*
 * Checksum routine for Internet Protocol family headers (C Version)
 */
unsigned short ComputeChecksum(unsigned char *data, int len){

         long sum = 0;  /* assume 32 bit long, 16 bit short */
	 unsigned short *temp = (unsigned short *)data;

         while(len > 1){
             sum += *temp++;
             if(sum & 0x80000000)   /* if high order bit set, fold */
               sum = (sum & 0xFFFF) + (sum >> 16);
             len -= 2;
         }

         if(len)       /* take care of left over byte */
             sum += (unsigned short) *((unsigned char *)temp);
          
         while(sum>>16)
             sum = (sum & 0xFFFF) + (sum >> 16);

        return ~sum;
}

void
CreateEthernetHeader(vrrp_vr_t *vr, unsigned char *buf, int buflen, int protocol)
{
	
	struct ethhdr *ethernet_header;
	ethernet_header = (struct ethhdr *)buf;
	
	//ethernet_header = (struct ethhdr *)malloc(sizeof(struct ethhdr));

	/* copy the Src mac addr */

	memcpy(ethernet_header->h_source,(uint8_t*)vrrp_vr_vmac(vr->vr_id, vr->vr_af), 6);

	/* copy the Dst mac addr */
	memcpy(ethernet_header->h_dest, phy_dst, 6);

	/* copy the protocol */

	ethernet_header->h_proto = htons(protocol);

	//printf("ethhdr proto: %d\n", htons(protocol));
	/* done ...send the header back */

	//return ((unsigned char *)ethernet_header);
}

/*function to create IP header*/
//void CreateIPHeader(vrrp_vr_t *vr, vrrp_attr_t *va, unsigned char *buf, int buflen){
void CreateIPHeader(vrrp_vr_t *vr, unsigned char *buf, int buflen){

	struct iphdr *ip_header = (struct iphdr *)buf;

	//ip_header = (struct iphdr *)malloc(sizeof(struct iphdr));

	ip_header->version = 4;
	ip_header->ihl = (sizeof(struct iphdr))/4 ;
	ip_header->tos = 0;
	ip_header->tot_len = htons(sizeof(struct iphdr));
	ip_header->id = 13579;
	ip_header->frag_off = 0;
	ip_header->ttl = VRRP_IP_TTL;
	ip_header->protocol = IPPROTO_VRRP;
		
	//ip_header->saddr = (in_addr_t)inet_addr(SRC_IP);
	ip_header->saddr = vr->in4;
	ip_header->daddr = (in_addr_t)inet_addr(INADDR_VRRP_GROUP);
	
	ip_header->check = 0;
	//compute checksum for the IP header
	//ip_header->check = ComputeChecksum((unsigned char *)ip_header, ip_header->ihl*4);

	//return ((struct iphdr*)ip_header);

}

/*function to create VRRP packet*/
//int CreateVRRPHeader(vrrp_vr_t *vr, vrrp_attr_t *va, unsigned char *buf, int buflen){
int CreateVRRPHeader(vrrpd_ds_t *p, unsigned char *buf, int buflen, int priority){

	/*int	i;
	VRRP_PKT *vp	= (VRRP_PKT *)buf;
	vp->ver		= (VRRP_VERSION << 4) | VRRP_PKT_ADVERT;
	vp->vrid	= vr->vr_id;
	vp->priority	= va->pri;
	vp->naddr	= vr->vr_ipnum;
	vp->auth_type	= VRRP_AUTH_NONE;
	vp->adver_int	= va->delay;
	
	uint32_t *ip = (uint32_t *)((char *)vp + sizeof (VRRP_PKT));
	uint32_t *addr = (uint32_t *)vr->vr_ip;
	for (i = 0; i < vr->vr_ipnum; i++) {
		ip[i] = htonl(addr[i]);
	}*/
	
	//vp->chksum=ComputeChecksum((unsigned char*)vp, vrrp_hdr_len(vr));

// 	memcpy(buf, p, sizeof(vrrpd_ds_t));	
	vrrp_inst_t *vi;
	vi = (vrrp_inst_t*)malloc(sizeof(vrrp_inst_t));

	vi->vi_va.pri = p->vr->attr.pri;

	if(priority == 0){
		vi->vi_va.pri = 0;
	}

	vi->vi_va.delay = p->vr->attr.delay;
	vi->vi_va.pree_mode = p->vr->attr.pree_mode;
	vi->vi_va.accept_mode = p->vr->attr.accept_mode;

	vi->vi_vr.vr_id = p->vr->vr.vr_id;
	vi->vi_vr.vr_af = p->vr->vr.vr_af;
	
	memcpy(&(vi->vi_vr.in4), &(p->vr->vr.in4), sizeof(in_addr_t));

	vi->vi_vr.vr_ipnum = p->vr->vr.vr_ipnum;

	strcpy(vi->vi_vr.vr_ifname, p->vr->vr.vr_ifname);

	vi->vi_state = p->state;
	vi->vi_active = p->active;

	memcpy(buf, vi, sizeof(vrrp_inst_t));

	return (sizeof(vrrp_inst_t));
}

/*Function to Send VRRP packets to a Multicast address*/

vrrp_ret_t SendVRRPPacket(int raw, unsigned char * pkt, int len){
	
	int sent=0;
	unsigned char ttl=255;
	
	/*set time to leave for a raw socket-it determines the life of the packet(for instance validity of the packet
          within LAN or in whole network) Here ttl=1 indicates life of packet is within a LAN*/
	if(!(setsockopt(raw, IPPROTO_IP, IP_MULTICAST_TTL, &ttl,sizeof(ttl))))
	{	
		vrrpd_log("Error setting socket option\n");
		return -1;
	}
	
	struct sockaddr_in dest_addr;

	bzero((struct sockaddr_in*)&dest_addr, sizeof(dest_addr));

	dest_addr.sin_family = AF_INET;
	dest_addr.sin_addr.s_addr = inet_addr(INADDR_VRRP_GROUP);
	
	#define VRRP_PORT 12345

	dest_addr.sin_port = htons(VRRP_PORT);

	if((sent = write(raw, pkt, len))!= len)
	//if((sent = sendto(raw, pkt, len, 0, (struct sockaddr*)&dest_addr, sizeof(struct sockaddr_in))) != len)
	{
		vrrpd_log("Writing error, Only %d bytes were written out of %d bytes. \n",sent,len);
		return -1;
	}

	//vrrpd_log("Packet transmission successful with packet size %d\n",sent);
	return VRRP_SUCCESS;

}

ParseEthernetHeader(unsigned char *packet, int len){

	struct ethhdr *ethernet_header;

	if(len > sizeof(struct ethhdr))
	{
		ethernet_header = (struct ethhdr *)packet;

		/* First set of 6 bytes are Destination MAC */

		//printhdr("Destination MAC: ", ethernet_header->h_dest, 6);
		printf("\n");
		
		/* Second set of 6 bytes are Source MAC */

		//printhdr("Source MAC: ", ethernet_header->h_source, 6);
		printf("\n");

		/* Last 2 bytes in the Ethernet header are the protocol it carries */

		//printhdr("Protocol: ",(void *)&ethernet_header->h_proto, 2);
		printf("\n");

		
	}
	else
	{
		printf("Packet size too small !\n");
	}
}

ParseIPHeader(unsigned char *packet, int len){

	struct ethhdr *ethernet_header;
	struct iphdr *ip_header;

	/* First Check if the packet contains an IP header using
	   the Ethernet header                                */

	ethernet_header = (struct ethhdr *)packet;

	//printf("ethhdr proto: %d\n", (ethernet_header->h_proto));
	if(ntohs(ethernet_header->h_proto) == ETH_P_IP)
	{
		/* The IP header is after the Ethernet header  */
		
		if(len >= (sizeof(struct ethhdr) + sizeof(struct iphdr)))
		{
			ip_header = (struct iphdr*)(packet + sizeof(struct ethhdr));
			
			/* print the Source and Destination IP address */
	
			//char *saddr,*daddr;

			//saddr = (char*)inet_ntoa(ip_header->saddr);
			//daddr = (char*)inet_ntoa(ip_header->daddr);
			printf("Dest IP address: %d\n", ip_header->saddr);
			printf("Source IP address: %d\n", ip_header->daddr);
			printf("TTL = %d\n", ip_header->ttl);	

		}
		else
		{
			printf("IP packet does not have full header\n");
		}

	}
	else
	{
		/* Not an IP packet */

	}
}

ParseVRRPHeader(unsigned char *packet, int len){

	struct ethhdr *ethernet_header;
	struct iphdr *ip_header;
	VRRP_PKT *vrppkt;

	/* First Check if the packet contains an IP header using
	   the Ethernet header                                */

	ethernet_header = (struct ethhdr *)packet;

	if(ntohs(ethernet_header->h_proto) == ETH_P_IP)
	{
		/* The IP header is after the Ethernet header  */
		
		ip_header = (struct iphdr*)(packet + sizeof(struct ethhdr));

		printf("iphdr proto: %d\n", (ip_header->protocol));
		if((ip_header->protocol) == 112)
		{

			printf("iphdr proto: %d\n", ntohs(ip_header->protocol));
			if(len >= (sizeof(struct ethhdr) + sizeof(struct iphdr) + sizeof(VRRP_PKT)))
			{

				vrppkt = (VRRP_PKT *)(packet + sizeof(struct ethhdr) + sizeof(struct iphdr));

				printf("VRRP header fields\n-------------------------------------\n\n");
				printf("Version: %d\n", (vrppkt->ver));
				printf("Type: %d\n", (vrppkt->type));
				printf("ID: %d\n", (vrppkt->vrid));
				printf("Priority: %d\n",(vrppkt->priority));
				printf("Number of IP addresses following: %d\n", (vrppkt->naddr));
				printf("Authentication Type: %d\n", (vrppkt->auth_type));
				printf("Advertisment Interval: %d\n", (vrppkt->adver_int));
				printf("Value of Checksum: %d\n", (vrppkt->chksum));

				unsigned short c ;//= ComputeChecksum((unsigned char*)vrppkt, sizeof(VRRP_PKT));
				//printf("cksum of received packet: %d\n", c);
					
				//verify VRRP checksum
				if(c==0)
					printf("RECEIVED PACKET IS CORRECT(NOT CORRUPTED)\n");
				else
					printf("RECEIVED PACKET IS INCORRECT(CORRUPTED)\n");

			}
			else
			{
				printf("VRRP packet does not have full header\n");
			}
		}	
	}
	else
	{
		/* Not an IP packet */

	}
}

/*Funtion to join Multicast group*/
extern int JoinMulticast(unsigned char * mulgrp,int rs){
	
	struct ip_mreq imreq;
	
	
	
	memset(&imreq, 0, sizeof(struct ip_mreq));

	imreq.imr_multiaddr.s_addr = inet_addr(mulgrp);
	imreq.imr_interface.s_addr = INADDR_ANY;


	if(!(setsockopt(rs, IPPROTO_IP, IP_ADD_MEMBERSHIP,(const void *)&imreq, sizeof(struct ip_mreq))))
	{
		printf("Joining multicast failed\n");
		return (-1);
	}

	return 0;
}


/*Function to leave Multicast group*/

extern int LeaveMulticast(unsigned char * mulgrp, int rs){

	struct ip_mreq imreq;
	
	
	
	memset(&imreq, 0, sizeof(struct ip_mreq));

	imreq.imr_multiaddr.s_addr = inet_addr(mulgrp);
	imreq.imr_interface.s_addr = INADDR_ANY;


	if(!(setsockopt(rs, IPPROTO_IP, IP_DROP_MEMBERSHIP,(const void *)&imreq, sizeof(struct ip_mreq))))
	{
		printf("Leaving Multicast failed\n");
		exit(-1);
	}

	return 0;	

}

PrintInHex(char *mesg, unsigned char *p, int len)
{
	printf("%s",mesg);

	while(len--)
	{
		printf("%.2X ", *p);
		p++;
	}

}
