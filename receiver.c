#include "header.h"
#include "vrrp.h"
//#include "multi.h"

int createsock(int protocol)
{
	int rs;

	
	if((rs = socket(PF_PACKET, SOCK_RAW, htons(protocol)))== -1)
	{
		perror("Error creating raw socket: ");
		exit(-1);
	}

	return rs;
}

int bindsock(char *device, int raw, int protocol)
{
	
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

void printpkt(unsigned char *packet, int len)
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

printhdr(char *mesg, unsigned char *p, int len)
{
	printf("%s",mesg);

	while(len--)
	{
		printf("%.2X ", *p);
		p++;
	}

}

parseethhdr(unsigned char *packet, int len)
{
	struct ethhdr *ethernet_header;

	if(len > sizeof(struct ethhdr))
	{
		ethernet_header = (struct ethhdr *)packet;

		/* First set of 6 bytes are Destination MAC */

		printhdr("Destination MAC: ", ethernet_header->h_dest, 6);
		printf("\n");
		
		/* Second set of 6 bytes are Source MAC */

		printhdr("Source MAC: ", ethernet_header->h_source, 6);
		printf("\n");

		/* Last 2 bytes in the Ethernet header are the protocol it carries */

		printhdr("Protocol: ",(void *)&ethernet_header->h_proto, 2);
		printf("\n");

		
	}
	else
	{
		printf("Packet size too small !\n");
	}
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

char *parseiphdr(unsigned char *packet, int len)
{
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
			
			if(ip_header->protocol == 112){
			
			
			
			struct in_addr in1, in2;
			memcpy(&(in1.s_addr), &(ip_header->daddr), sizeof(unsigned long));
			memcpy(&(in2.s_addr), &(ip_header->saddr), sizeof(unsigned long));
			
			char *dstip;
			dstip = (char*)malloc(sizeof(char)*20);
			strcpy(dstip, (char*)inet_ntoa(in1));

			if(strcmp(inet_ntoa(in1), "224.0.0.18")){		
				printf("Dest IP address: %s\n", inet_ntoa(in1));
				printf("Source IP address: %s\n", inet_ntoa(in2));
				printf("TTL = %d\n", ip_header->ttl);	
				parseethhdr(packet, len);
				return dstip;
			}

			else return NULL;
			
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


int main(int argc, char **argv)
{
	int raw;
	unsigned char packet_buffer[2048]; 
	int len, dlen;
	int packets_to_sniff;
	struct sockaddr_ll packet_info;
	int packet_info_size = sizeof(packet_info_size);
	
	char * data;
	char *dst = NULL;
	
	
	

	/* create the raw socket */

	raw = createsock(ETH_P_IP);

	/* Bind socket to interface */

	bindsock(argv[1], raw, ETH_P_IP);

	/* Start Sniffing and print Hex of every packet */
	
		
	while(1)
	{
		bzero(packet_buffer, 2048);
		if((len = recvfrom(raw, packet_buffer, 2048, 0, (struct sockaddr*)&packet_info, &packet_info_size)) == -1)
		{
			perror("Recv from returned -1: ");
			exit(-1);
		}
		else
		{
			/* Parse IP Header */

			dst = parseiphdr(packet_buffer, len);
			
			if(dst == NULL)continue;

			//if(!strcmp(dst, "224.0.0.18"))continue;
			
			printf("\n\n\nReceived packet of length : %d\n",len);
			
			data = (char *)packet_buffer;
			
			data += (sizeof(struct ethhdr) + sizeof(struct iphdr));
			
			dlen = strlen(data);
			data[dlen] = '\0';

			printf("Data : %s\n\n",data);
			
			printf("\n\n-----------------------------------------------------------------\n\n");
			
		}
	}
	
	
	return 0;
}

