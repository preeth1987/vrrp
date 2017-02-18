#include "header.h"
#include<linux/tcp.h>

#define SRC_ETHER_ADDR	"aa:aa:aa:aa:aa:aa"
#define DST_ETHER_ADDR	"00:15:f2:3b:1f:13"
#define SRC_IP	"127.0.0.1"
#define MASTER_IP "10.10.10.10"

char *dst_eth;

void *acpt_dest_eth(void*);

char * GetMasterEthAddr(unsigned char* pkt,int len);

char * IPDstAddr(unsigned char * packet,int len);

pthread_mutex_t dest_eth_lock;

int CreateRawSocket(int protocol)
{
	int sd;
	
	if((sd=socket(PF_PACKET, SOCK_RAW, htons(protocol))) == -1)
	{
		perror("Error creating the socket\n");
		exit (-1);
	}
	
	return sd;
}


int BindRawSocketToInterface(int s, char * device, int protocol)
{
	struct sockaddr_ll s1;
	struct ifreq ifr;

	bzero(&s1,sizeof(s1));
	bzero(&ifr,sizeof(ifr));

	strncpy((char *)ifr.ifr_name,device,IFNAMSIZ);
	if((ioctl(s,SIOCGIFINDEX,&ifr))==-1)
	{
		perror("Error getting interface\n");
		exit(-1);
	}
	
	

	s1.sll_family=AF_PACKET;
	s1.sll_ifindex = ifr.ifr_ifindex;
	s1.sll_protocol = htons(protocol);


	
	if((bind(s, (struct sockaddr *)&s1, sizeof(s1)))== -1)
	{
		perror("Error binding raw socket to interface\n");
		exit(-1);
	}

	return 1;
	
}



unsigned char * CreateEthernetHeader(char * src, char * dst, int protocol)
{
	struct ethhdr * eth;
	

	eth = (struct ethhdr *) malloc (sizeof(struct ethhdr));


	memcpy(eth->h_source, (void *)ether_aton(src), 6);

	pthread_mutex_lock(&dest_eth_lock);
	memcpy(eth->h_dest, (void *)ether_aton(dst), 6);
	pthread_mutex_unlock(&dest_eth_lock);
	
	eth->h_proto = htons(protocol);
	
	printf("ethhdr proto: %d\n", htons(protocol));
	
	return ((unsigned char *)eth);


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



struct iphdr * CreateIP(char *dst)
{
	struct iphdr * ip;

	ip = (struct iphdr *) malloc (sizeof(struct iphdr));

	ip->version = 4;
	ip->ihl = (sizeof(struct iphdr))/4 ;
	ip->tos = 0;
	ip->tot_len = htons(sizeof(struct iphdr));
	ip->id = htons(111);
	ip->frag_off = 0;
	ip->ttl = 111;
	ip->protocol = (112);
		
	ip->saddr = (in_addr_t)inet_addr(SRC_IP);
	ip->daddr = (in_addr_t)inet_addr(dst);
	
	ip->check = 0;
	
	ip->check = ComputeChecksum((unsigned char *)ip, ip->ihl*4);

	return ((struct iphdr*)ip);

}

	



	
int main(int argc, char * argv[])
{

	int rs,len,dlen;
	
	pthread_t ptid;
	
	unsigned char * eth;
	unsigned char * packet;
	struct iphdr * ip;
	int pktlen,sent;
	char data[1000];
	char dest[100];

	dst_eth = (char*)malloc(sizeof(char)*25);
	
	pthread_create(&ptid, 0, acpt_dest_eth, (char*)argv[1]);
	
	rs=CreateRawSocket(ETH_P_ALL);

	BindRawSocketToInterface(rs,argv[1],ETH_P_ALL);

	while(1){

		printf("\n\n----------------------------------------------------\n\n");


		bzero(data, 1000);
		printf("Enter the message you want to send:\n");
		scanf(" %[^\n]s",data);
		printf("Enter the destination ip:\n");
		scanf(" %s",dest);
	
	
		char * buf;
		//buf = strcat(buf,dest);

		ip = (struct iphdr*)CreateIP(dest);
		
		printf("SENDING MESSAGE TO %s ......\n", dest);
		pthread_mutex_lock(&dest_eth_lock);
		strcpy(dst_eth,"00:00:00:00:00:00");
		pthread_mutex_unlock(&dest_eth_lock);
		sleep(1);

		if(!strcmp(dst_eth, "00:00:00:00:00:00")){

			printf("ERROR IN SENDING PACKET -- ROUTER DOWN\n\n");continue;
		}

		eth = CreateEthernetHeader(SRC_ETHER_ADDR, dst_eth, ETH_P_IP);
	
	

		pktlen = sizeof(struct ethhdr) + sizeof(struct iphdr) + strlen(data);
	
		dlen = strlen(data);
	
		data[dlen] = '\0';
	
		packet = (unsigned char *)malloc(sizeof(pktlen));
		
		
		memcpy(packet,eth,sizeof(struct ethhdr));
	
		memcpy((packet + sizeof(struct ethhdr)),ip,sizeof(struct iphdr));
		
		memcpy((packet + sizeof(struct ethhdr) + sizeof(struct iphdr)),data,strlen(data));

	

		printf("PACKET LENGTH: %d\n", pktlen);
		printf("ETHERNET HEADER LENGTH: %d\n", sizeof(struct ethhdr));
		printf("IP HEADER LENGTH: %d\n", sizeof(struct iphdr));
		printf("DATA SENT: %s\n",data);

		//pktlen = sizeof(struct ethhdr) + sizeof(struct iphdr) + strlen(data);
	
		if((sent=send(rs, packet, pktlen, 0)) == -1)
		{
			perror("Error sending the data to Master.\n");
			exit (-1);
		}
		
		//pthread_mutex_lock(&dest_eth_lock);
		//strcpy(dst_eth,"00:00:00:00:00:00");
		//pthread_mutex_unlock(&dest_eth_lock);

		
		printf("%d bytes of data sent to Master.\n",sent);
	
		
	}
	
	close(rs);
	
	pthread_join(ptid, NULL);
	
	return 0;
}

void *acpt_dest_eth(void* intf){

	int raw,ret,port;
	unsigned char packet_buffer[2048]; 
	int len;
	int packets_to_sniff;
	struct sockaddr_ll packet_info;
	int packet_info_size = sizeof(packet_info);
	
	char *dst_ip,*src_eth;
	
	raw=CreateRawSocket(ETH_P_ALL);
	
	BindRawSocketToInterface(raw,intf,ETH_P_ALL);
	
	while(1){
	
		

		len=recvfrom(raw,packet_buffer,4096,0,(struct sockaddr*)&packet_info,&packet_info_size);
		if(len<0)
			continue;
	
		dst_ip=(char *)IPDstAddr(packet_buffer,len);
		
		if(!strcmp(dst_ip,"192.168.53.255")){
		
			port = TCP_Port(packet_buffer, len);

			if(port == 9099){
				src_eth=(char*)GetMasterEthAddr(packet_buffer,len);
				
				//printf("source ETHERNET :%s\n",src_eth);
			
				pthread_mutex_lock(&dest_eth_lock);
				strcpy(dst_eth,src_eth);
				pthread_mutex_unlock(&dest_eth_lock);
			}
				
		}
	}
}

char * GetMasterEthAddr(unsigned char* pkt,int len){

	struct ethhdr *eth_hdr=(struct ethhdr *)pkt;
	char *dst_eth;
	
	dst_eth=(char *)malloc(sizeof(char)*25);
	
	dst_eth=(char *)ether_ntoa(eth_hdr->h_source);
	
	return dst_eth;
}

char * IPDstAddr(unsigned char * packet,int len){

	struct iphdr *ip_hdr;
	
	ip_hdr=(struct iphdr*)(packet+sizeof(struct ethhdr));
	
	struct in_addr in;
		
	memcpy(&(in.s_addr), &(ip_hdr->daddr), sizeof(unsigned long));
	
	char *dstip;
	
	dstip = (char*)malloc(sizeof(char)*25);
	
	strcpy(dstip, (char*)inet_ntoa(in));
	
	return dstip;
}

int TCP_Port(unsigned char* packet, int len){

	struct tcphdr *tcp_header;	
	struct iphdr *ip_header;

	ip_header = (struct iphdr*)(packet + sizeof(struct ethhdr));
	
	tcp_header = (struct tcphdr*)(packet + sizeof(struct ethhdr) + ((ip_header->ihl)*4) );
	
	int dst_port = tcp_header->dest;

	return dst_port;
}
