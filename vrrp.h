/*
	VRRP header file: This header defines various errors,vrrp packet structure etc....
*/

#ifndef __VRRP_H__
#define __VRRP_H__

#ifdef __cplusplus
extern "C"{
#endif

#define vrrpd_log printf

/*VRRP PACKET STRUCTURE*/
typedef struct {
uint8_t		ver;
uint8_t		type;
uint8_t		vrid;
uint8_t		priority;
uint8_t		naddr;	
uint8_t		auth_type;
uint8_t		adver_int;
uint16_t 	chksum;
/* then follows <naddr> IPv4 addresses */
}VRRP_PKT;

#define IPPROTO_VRRP 112
#define INADDR_VRRP_GROUP "224.0.0.18"	/* IPv4 multicast address 224.0.0.18 as defined in RFC*/

/* length of a chunk of Auth Data */
#define VRRP_AUTH_LEN   0

#define VRRP_IP_TTL	255		/* IPv4 TTL */
#define VRRP_VERSION	2		/* current version */
#define VRRP_PKT_ADVERT	1		/* packet type */
#define VRRP_PRIO_OWNER	255		/* priority of IP address owner */
#define VRRP_PRIO_DFL	100		/* default priority */
#define VRRP_PRIO_STOP	0		/* priority to stop */
#define VRRP_AUTH_NONE	0		/* no authentication */
#define VRRP_AUTH_RES1	1		/* reserved */
#define VRRP_AUTH_RES2	2		/* reserved */
#define VRRP_ADVER_DFL	1		/* default ad interval (in sec) */
#define VRRP_PREEMPT_DFL 1		/* default preempt mode */


/* VRRP state machine */
#define VRRP_STATE_INIT	1
#define VRRP_STATE_MAST 2
#define VRRP_STATE_BACK 3
#define VRRP_STATE_NONE	0


#ifdef __cplusplus
}
#endif

#endif /*__VRRP_H__*/
