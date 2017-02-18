/*
	libvrrp.h: header file for libvrrp.c
*/

#ifndef __LIBVRRP_H__
#define __LIBVRRP_H__


#include "vrrp.h"
#include <time.h>

#ifdef __cplusplus
extern "C"{
#endif

#define CMDMAXLEN 255
#define BUFSIZE 65535
/*Unix domain socket name used for communication between vrrpd and vrrpadm*/
#define VRRPADM_VRRPD_SOCKET "/tmp/vrrpadm-d.socket"
#define VRRPD_VRRPADM_SOCKET "/tmp/vrrpd-adm.socket"

#define LIFNAMSIZ 32

#define BOOL(b)         ((b) == TRUE ? "true" : \
                            ((b) == FALSE ? "false" : "unknown"))

/*types used*/
typedef uint8_t vrpri_t;	/* VR priority, 0-255 */
typedef uint8_t vrid_t;		/* VRID, 0-255 */
typedef uint8_t vrrp_state_t;	/* VRRP state */

typedef enum{
	FALSE=0,
	TRUE,
}boolean_t;

typedef struct{

	char intf[32];
	int s_send;/*for sending IPv4 adv */
	int s_rcv;/*for receving IPv4 adv */
}vrrp_intf_fd_t;

typedef struct vrrp_vr {	/* virtual router */
	vrid_t	vr_id;		/* VRID of this vr */
	int	vr_af;		/* VRIP family */
	union {			/* primary IP */
		in_addr_t	in4;
		//in6_addr_t	in6;
	} vr_pip;

#define	in4	vr_pip.in4
//#define	vr_pip6	vr_pip.in6

	uint8_t	vr_ipnum;	/* number of VRIP, 1-255 */
	void	*vr_ip;		/* pointer to the VRIP array */
	char	vr_ifname[32];
} vrrp_vr_t;

typedef struct vrrp_attr {
	vrpri_t		pri;		/* priority */
	int		delay;		/* advertisement interval */
	boolean_t	pree_mode;	/* preempt mode */
	boolean_t	accept_mode;	/* accept mode */
} vrrp_attr_t;

typedef struct vrrp_inst {
	vrrp_attr_t	vi_va;		/* vrrp attributes */
	vrrp_vr_t	vi_vr;		/* virtual router attribute set */
	vrrp_state_t	vi_state;	/* state */
	boolean_t	vi_active;	/* start up? */
} vrrp_inst_t;

typedef struct vrrp_status {
	vrrp_state_t	current_state;
	vrrp_state_t	previous_state;
	struct timeval	state_trans_time;	/* timestamp of last state transition */
} vrrp_status_t;

/*
 * Information retrieved from the last adv message I receives from a peer
 * Usually the peer is a MASTER
 */
typedef struct {
	int	af;	
	struct in_addr	addr;		/* Source IP addr of the message */        
	struct timeval	timestamp;	/* timestamp of the adv message */
	vrpri_t		priority;	/* priority in adv message */
	int 		interval;	/* adv_interval in the adv message */
					/* also my master_adv_interval */
} vrrp_vr_advinfo_t;

/*Information retrieved from the last adv message from a MASTER*/

typedef struct {
	/* Fields directly retrieved from the received adv message. They're attributes of the MASTER */
	int adv_af;			/* address family */
	struct in_addr  adv_addr;	/* source IP addr of the message */
	struct timeval	adv_timestamp;	/* timestamp of the message */
	vrpri_t		adv_priority;	/* priority in the adv message */
	int		adv_interval;	/* interval in the adv message */
	int		adv_age;	/* time elapsed since last message */

	/*
	 * Fields calculated from the received adv message
	 * They are peers attributes, although calculated from peer's adv
	 * not available if in the MASTER state
	 */ 
	int	skew_time;		/* skew_time */
	int	master_down_interval;	/* master_down_interval */
	int	master_down_timer;	/* master_down_timer */
} vrrp_advinfo_t;

/*VRRP property changes are indicated using these macros*/
#define	VR_PROP_UNCHANGE	-1
#define	VR_PROP_PREEMPT		1
#define	VR_PROP_UNPREEMPT	0
#define	VR_PROP_ACCEPTED        1
#define	VR_PROP_NOTACCEPTED	0

/* These commands are used between vrrpadm and vrrpd */

typedef enum {
	CMD_RETURN_VAL = 0,
	CMD_CREATE_INST,
	CMD_REMOVE_INST,
	CMD_STARTUP_INST,
	CMD_SHUTDOWN_INST,
	CMD_MODIFY_INST,
	CMD_SHOW_ALL,
	CMD_GET_INSTX,
}vrrp_cmd_t;

/*VRRP return types*/
typedef enum {
	VRRP_SUCCESS = 0,
	VRRP_EVREXIST,	/* vrrp instance already exists */
	VRRP_EINVAL,	/* invalid parameter */
	VRRP_ENOROUTE,	/* cannot determine interface by vr_pip */
	VRRP_EVRIP,	/* invalid virtual router ip */
	VRRP_ENOINST,	/* vrrp instance does not exist */
	VRRP_EMULINST,	/* more than one vrrp instances are found */
	VRRP_ENOMEM,	/* malloc failed */
	VRRP_ESYS	/* system error */
} vrrp_ret_t;

static char* vrrp_rets[]={
	"VRRP_SUCCESS",
	"VRRP_EVREXIST",
	"VRRP_EINVAL",
	"VRRP_ENOROUTE",
	"VRRP_EVRIP",
	"VRRP_ENOINST",
	"VRRP_EMULINST",
	"VRRP_ENOMEM",
	"VRRP_ESYS"
};

/*state change of VRRP instances are indicated using these macros*/
typedef enum {
	BACK_TO_MAST = 0,
	MAST_TO_BACK,
	INIT_TO_MAST,
	INIT_TO_BACK,
	MAST_TO_INIT,
	BACK_TO_INIT,
} vrrp_state_trans_t;

/* The corresponding states are defined in vrrp.h */
static char *vrrp_stats[] = {
        "VRRP_STATE_NONE",
        "VRRP_STATE_INIT",
        "VRRP_STATE_MAST",
        "VRRP_STATE_BACK"
};

/*these data structures required for implementation of vrrpd functions (i.e., vrrp_create_inst....)*/

typedef useconds_t vrrp_timeout_t;

typedef struct vrrpd_vr_ds{

	vrrp_attr_t attr;
	vrrp_vr_t vr;
	vrrp_intf_fd_t *vfd;//file descriptors for each VRRP to send and receive advertisements
	vrrp_timeout_t timeout;

	vrrp_vr_advinfo_t advinfo;

	

#define ai_af advinfo.af
#define ai_addr advinfo.addr
#define ai_timestamp advinfo.timestamp
#define ai_priority advinfo.priority
#define ai_interval advinfo.interval

	struct vrrpd_ds	 	*parent;//points to instance which contains current VR
	struct vrrpd_vr_ds	*next;//points to next VR in the list
	struct vrrpd_vr_ds	*prev;//points to previous VR in the list
	/*These pointers to VR are required in advertisement queues*/

	/*advertisement queue adv_q is a queue of VR(implemented as list) running in MASTER & BACUP STATE 
          (i.e., VR's capable of sending and receiving advertisement & whose timer are running waiting for advertisement,
	  if adv not received the backup router transforms to master state)*/
	struct vrrpd_vr_ds	*qnext;
	struct vrrpd_vr_ds	*qprev;

}vrrpd_vr_ds_t;

typedef struct vrrpd_ds{

	vrrpd_vr_ds_t *vr;/*pointer to list*/
		
	boolean_t active;
	vrrp_state_t state;

	pid_t pid; //hold pid of routing process
		
	vrrp_state_t previous_state;
	
	struct timeval state_trans_time; //last state time
	
	struct vrrpd_ds *next;
	struct vrrpd_ds *prev;

}vrrpd_ds_t;

/*declare vrrp_strerror() which returns the char* to error string for the error value passed as argument*/
char* vrrp_strerror(int);

char* timeval_to_string(struct timeval *tv, char *buf, size_t buflen);

void display_vrrp_instx(vrrpd_ds_t *v);

void dump_vrrp_vr(vrrpd_vr_ds_t *v);

void dump_vr(vrrp_vr_t *vr);

void dump_vrrp_attr(vrrp_attr_t *va);

void free_vrrp(vrrpd_ds_t *im);

#ifdef __cplusplus
}
#endif

#endif /*__LIBVRRP_H__*/
