#include "header.h"
#include "vrrp.h"

#define SRC_MAC "00:e0:4d:84:84:c8"
#define SRC1_IP "192.168.100.22"

void* SendBcastPacket();

int CreateRawSocket(int protocol){
	int sock;
	
	if((sock=socket(PF_PACKET,SOCK_RAW,htons(protocol)))==-1)
	{
		perror("Creating socket failed\n");
		exit (-1);
	}
	return sock;
}

int BindRawSocketToInterface(int s,char * device,int protocol){

	struct sockaddr_ll s1;
	struct ifreq ifr;

	bzero(&s1,sizeof(s1));
	bzero(&ifr,sizeof(ifr));

	strncpy((char *)ifr.ifr_name,device,IFNAMSIZ);
	if((ioctl(s,SIOCGIFINDEX,&ifr))==-1)
	{
		printf("Error getting interface\n");
		exit(-1);
	}
	
	

	s1.sll_family=AF_PACKET;
	s1.sll_ifindex = ifr.ifr_ifindex;
	s1.sll_protocol = htons(protocol);


	
	if((bind(s, (struct sockaddr *)&s1, sizeof(s1)))== -1)
	{
		printf("Error binding raw socket to interface\n");
		exit(-1);
	}

	return 1;
	
}

unsigned char* CreateEthernetHeader(char *src_mac, char *dst_mac, int protocol)
{
	struct ethhdr *ethernet_header;

	
	ethernet_header = (struct ethhdr *)malloc(sizeof(struct ethhdr));

	/* copy the Src mac addr */

	memcpy(ethernet_header->h_source, (void *)ether_aton(src_mac), 6);

	/* copy the Dst mac addr */
	memcpy(ethernet_header->h_dest, (void *)ether_aton(dst_mac), 6);

	/* copy the protocol */

	ethernet_header->h_proto = htons(protocol);

	printf("ethhdr proto: %d\n", htons(protocol));
	/* done ...send the header back */

	return ((unsigned char *)ethernet_header);


}

unsigned short ComputeChecksum(unsigned char *data, int len)
{
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

/*function to create IP header*/
struct iphdr* CreateIPHeader(char* SRC_IP, char* DST_IP)
{
	struct iphdr *ip_header;

	ip_header = (struct iphdr *)malloc(sizeof(struct iphdr));

	ip_header->version = 4;
	ip_header->ihl = (sizeof(struct iphdr))/4 ;
	ip_header->tos = 0;
	ip_header->tot_len = htons(sizeof(struct iphdr));
	ip_header->id = htons(111);
	ip_header->frag_off = 0;
	ip_header->ttl = 111;
	ip_header->protocol = (112);
		
	ip_header->saddr = (in_addr_t)inet_addr(SRC_IP);
	ip_header->daddr = (in_addr_t)inet_addr(DST_IP);
	
	ip_header->check = 0;
	//compute checksum for the IP header
	//ip_header->check = ComputeChecksum((unsigned char *)ip_header, ip_header->ihl*4);

	return ((struct iphdr*)ip_header);

}

void PrintPacketInHex(unsigned char *packet, int len)
{
	unsigned char *p = packet;

	printf("\n\n---------Packet---Starts----\n\n");
	
	while(len--)
	{
		printf("%.2x ", *p);
		p++;
	}

	printf("\n\n--------Packet---Ends-----\n\n");

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


ParseEthernetHeader(unsigned char *packet, int len, struct ethhdr* ethernet_header)
{
	//struct ethhdr *ethernet_header;

	if(len > sizeof(struct ethhdr))
	{
		ethernet_header = (struct ethhdr *)packet;

		/* First set of 6 bytes are Destination MAC */

		PrintInHex("Destination MAC: ", ethernet_header->h_dest, 6);
		
		printf("\n");
		
		/* Second set of 6 bytes are Source MAC */

		PrintInHex("Source MAC: ", ethernet_header->h_source, 6);	
		
		printf("\n");

		/* Last 2 bytes in the Ethernet header are the protocol it carries */

		PrintInHex("Protocol: ",(void *)&ethernet_header->h_proto, 2);
		printf("\n");

		
	}
	else
	{
		printf("Packet size too small !\n");
	}
}

char* ParseIpHeader(unsigned char *packet, int len, struct iphdr *ip_header)
{
	struct ethhdr *ethernet_header;
	//struct iphdr *ip_header;

	/* First Check if the packet contains an IP header using
	   the Ethernet header                                */

	ethernet_header = (struct ethhdr *)packet;

	if(ntohs(ethernet_header->h_proto) == ETH_P_IP)
	{
		/* The IP header is after the Ethernet header  */
		
		if(len >= (sizeof(struct ethhdr) + sizeof(struct iphdr)))
		{
			ip_header = (struct iphdr*)(packet + sizeof(struct ethhdr));
			
			if(ip_header -> protocol == 112){
			
			struct ethhdr* eth;
	
			eth = (struct ethhdr*)malloc(sizeof(struct ethhdr));
	
			ParseEthernetHeader(packet, len, (struct ethhdr*) eth);
			
			struct in_addr in;
			memcpy(&(in.s_addr), &(ip_header->daddr), sizeof(unsigned long));
			char *dstip;
			dstip = (char*)malloc(sizeof(char)*20);
			strcpy(dstip, (char*)inet_ntoa(in));
			//printf("Destination IP: %s\n", (char*)inet_ntoa(in));
			printf("Destination IP: %s\n", dstip);
			
			printf("TTL = %d\n", ip_header->ttl);	
			
			return (char*)dstip;
			}
			else return NULL;

		}
		else
		{
			printf("IP packet does not have full header\n");
			return NULL;
		}

	}
	else
	{
		/* Not an IP packet */
		return NULL;

	}
}

int SendRawPacket(int rawsock, unsigned char *pkt, int pkt_len){

	int sent= 0;

	/* A simple write on the socket ..thats all it takes ! */

	if((sent = write(rawsock, pkt, pkt_len)) != pkt_len)
	{
		/* Error */
		printf("Could only send %d bytes of packet of length %d\n", sent, pkt_len);
		return 0;
	}

	return 1;
	

}

void SIGINT_handler(int sig){

	printf("From SIGINT: just got a %d (SIGINT ^C) signal\n", sig);
	printf("ROUTER GOING DOWN\n");
	printf("PID: %d killed\n", getpid());
	//kill(SIGCHLD,getppid());
	exit(0);
	
}
struct ip_mac{
	unsigned char mac[20];
	unsigned char ip[20];
};

struct ip_mac IP_MAC[] = {
	{"00:15:f2:3b:20:47","172.16.101.6"},
	{"00:15:f2:3b:20:9c","172.16.103.64"},
	{"00:15:f2:3b:1f:65","172.16.101.7"},
	{"00:15:f2:3b:20:43","172.16.101.5"},
	{"00:15:F2:3B:1F:13","172.16.102.17"},
	{"ff:ff:ff:ff:ff:ff","255.255.255.255"},
	{"00:e0:4d:84:84:c8","192.168.1.6"},
	{"00:23:4d:4e:96:1c","192.168.1.4"},
	{"00:1b:77:a7:86:f7","192.168.1.3"},
	{"00:23:8b:08:1f:15","192.168.1.10"}
};


//sudo router <receiving interface> <sending interface>
int main(int argc, char **argv)
{
	int raw_rcv, raw_snd;
	unsigned char packet_buffer[2048]; 
	int len, i;
	int packets_to_route;
	struct sockaddr_ll packet_info;
	int packet_info_size = sizeof(packet_info_size);
	char* data;

	
	struct iphdr* ip;
	
	ip = (struct iphdr*)malloc(sizeof(struct iphdr));
	unsigned char *eth1;
	struct iphdr *ip1;

	unsigned char* packet;
	
	//packet = (unsigned char*)malloc(sizeof(char)*2048);
	struct in_addr s_ip_addr, d_ip_addr;
	unsigned char phy_dst[20];
	unsigned char *ip_dst;
	ip_dst = (char*)malloc(sizeof(char) * 20);

	unsigned char* src_eth ;
	
	src_eth = (char*)malloc(sizeof(char)*20);

	unsigned char* src_ip;

	src_ip = (char*)malloc(sizeof(char)*20);

	strcpy(src_ip, SRC1_IP);
	
	strcpy(src_eth, SRC_MAC);
	int dlen, pktlen;
	char *phy_src;
	
	phy_src = (char*)malloc(sizeof(char)*20);
	strcpy(phy_src,"aa:aa:aa:aa:aa:aa");
	
	struct in_addr in;
	

	/* create the raw socket */

	signal(SIGINT, SIGINT_handler);
	pthread_t ptid;

	//send packet over broadcast to indicate that I am router 
	pthread_create(&ptid, 0,SendBcastPacket, NULL);


	raw_rcv = CreateRawSocket(ETH_P_IP);
	raw_snd = CreateRawSocket(ETH_P_IP);

	

	printf("ROUTER IFNAME: %s\nROUTER VMAC: %s\n", argv[1], argv[4]);

	if(argv[4] != NULL)
		strcpy(phy_src, argv[4]);

	//printf("\nphy_src: %s\n", phy_src);
	BindRawSocketToInterface(raw_rcv, argv[1], ETH_P_IP);
	BindRawSocketToInterface(raw_snd, argv[1], ETH_P_IP);

	

	/* Get number of packets to sniff from user */

	//packets_to_route = atoi(argv[2]);

	/* Start Sniffing and print Hex of every packet */
	
	while(1)
	{
		bzero(packet_buffer, 2048);
		if((len = recvfrom(raw_rcv, packet_buffer, 2048, 0, (struct sockaddr*)&packet_info, &packet_info_size)) == -1)
		{
			perror("Recv from returned -1: ");
			continue;
		}
		else
		{
			
			/* Parse IP Header */

			ip_dst = (char*)ParseIpHeader(packet_buffer, len, (struct iphdr*) ip);
			
			if(ip_dst == NULL) continue;
			//ip = (struct iphdr*)(packet + sizeof(struct ethhdr));
			
			printf("\n\n----------------------------------------------------------------------\n\n");

			//obtain data
			data = (char*)(packet_buffer);
			data += (sizeof(struct ethhdr) + sizeof(struct iphdr));
	
			dlen = strlen(data);
			data[dlen] = '\0';
			printf("\nDATA: %s\n", data);

			

			
			in.s_addr = ip->daddr;
			//ip_dst = inet_ntoa(in);
			printf("dst ip: %s\n", ip_dst);
	
			//obtain MAC of destination IP d_ip_addr
			for(i=0;i<20;i++){
				if(strcmp(ip_dst, IP_MAC[i].ip) == 0){
					strcpy(phy_dst, IP_MAC[i].mac);
					break;
				}
			}
			
			printf("dst eth addr: %s\n", phy_dst);
	
			//build custom headers to send to proper destination

			eth1 = CreateEthernetHeader(phy_src, (char*)phy_dst, ETH_P_IP);

			ip1 = (struct iphdr*)CreateIPHeader(src_ip, ip_dst);
			
			pktlen = sizeof(struct ethhdr) + sizeof(struct iphdr) + dlen;

			//bzero(packet, pktlen);
			
			packet = (unsigned char *)malloc(sizeof(pktlen));
			
			memcpy(packet,eth1,sizeof(struct ethhdr));

			memcpy((packet + sizeof(struct ethhdr)),ip1,sizeof(struct iphdr));

			memcpy((packet+ sizeof(struct ethhdr) + sizeof(struct iphdr)) ,data, strlen(data));
			
			if(!SendRawPacket(raw_snd, packet, pktlen)){
				perror("Error sending packet");
			}
			else
				printf("Packet sent successfully\n");
			
		}
	}
	pthread_join(ptid, NULL);

}

//send bcast packet

#define PORT 9099
#define DEST_ADDR "192.168.53.255"

VRRP_PKT* CreateVRRPPkt(){

	VRRP_PKT *vrrppkt;
	vrrppkt=(VRRP_PKT *)malloc(sizeof(VRRP_PKT));

	vrrppkt->ver = 2;
	vrrppkt->type = 1;
	vrrppkt->vrid = 10;
	vrrppkt->priority = 100;
	vrrppkt->naddr = 10;
	vrrppkt->auth_type = 0;
	vrrppkt->adver_int = 1;
	vrrppkt->chksum = 0;
	//compute checksum for the VRRP header
	vrrppkt->chksum = ComputeChecksum((unsigned char *)vrrppkt,sizeof(VRRP_PKT));

	return vrrppkt;
}

void* SendBcastPacket(){

	int sockfd;
	int broadcast=1;
	struct sockaddr_in sendaddr;
	struct sockaddr_in recvaddr;
	int numbytes;
	
	if((sockfd = socket(PF_INET,SOCK_DGRAM,0)) == -1)
	{
		perror("sockfd");
		exit(1);
	}
	
	if((setsockopt(sockfd,SOL_SOCKET,SO_BROADCAST,
				&broadcast,sizeof broadcast)) == -1)
	{
		perror("setsockopt - SO_SOCKET ");
		exit(1);
	}

        	
	sendaddr.sin_family = AF_INET;
	sendaddr.sin_port = PORT;
	sendaddr.sin_addr.s_addr = INADDR_ANY;
	memset(sendaddr.sin_zero,'\0',sizeof sendaddr.sin_zero);
	
	if(bind(sockfd, (struct sockaddr*) &sendaddr, sizeof sendaddr) == -1)
	{
		perror("bind");
		exit(1);
	}
	
	int i=0;
	while(1){
		recvaddr.sin_family = AF_INET;
		recvaddr.sin_port = PORT;
		recvaddr.sin_addr.s_addr = inet_addr(DEST_ADDR);
		memset(recvaddr.sin_zero,'\0',sizeof recvaddr.sin_zero);
	
		VRRP_PKT *vrrppkt = CreateVRRPPkt();
	
// 	while(1){
		if((numbytes = sendto(sockfd, (VRRP_PKT*)&vrrppkt, sizeof(VRRP_PKT) , 0, (struct sockaddr *)&recvaddr, sizeof recvaddr)) != -1){
			//printf("Sent a Bcast packet of %d bytes\n", numbytes);
			sleep(1);
			i++;
		}
		//perror("sendto");
		free(vrrppkt);
        	//exit(1);		
	}
	close(sockfd);
	
	return 0;

}
