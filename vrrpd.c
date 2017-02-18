/***********
VRRP Daemon Code
***********/

#include "header.h"

#include "vrrp.h"
#include "libvrrp.h"
/*defined in vrrpd_impl.c*/
//extern vrrpd_ds_t adv_q;
#include "vrrpd_impl.h"

/* Poll support */
static int              pollfd_num = 0;
static struct pollfd    *pollfds = NULL;
static char		poll_intfs[32][LIFNAMSIZ];
static int		poll_afs[32];
	/* TBD: poll-intfs should use same mechanism as pollfds */

/* should use linked list instead? */
static vrrp_intf_fd_t	netfds[16];


/*list of inatances @ runtime*/
extern vrrpd_ds_t inst_runtime_list;
/*list of VR's @ runtime*/
extern vrrpd_vr_ds_t vr_runtime_list;

//advertisement queue
extern vrrpd_vr_ds_t  *adv_q;

//to lock operations into adv_q
pthread_mutex_t adv_q_lock;

int flag[256];

void vrrpd_adm_thread();
int make_daemon();
int setup_adm_socket();
void process_adm_sock(int);
void signal_alarm(int );

int build_vrrp_ret(uint8_t *buf, vrrp_ret_t rval);
int build_vrrp_ret_get_instx(uint8_t *buf, vrrpd_ds_t *v, vrid_t next_vid, const char *next_intf, int next_af);
int build_vrrp_vr_buf(uint8_t *buf, vrrpd_vr_ds_t *v, int num);
unsigned short ComputeChecksum(unsigned char *data, int len);

void process_vrrpd_sock(int s, int af, const char* ifname);
int vrrp_recv_adv4(int sock, uint8_t *buf, int *buflen);
//VRRP_PKT *vrrp_adv_check(uint8_t *buf, const int buflen, const char* intf, struct sockaddr *from);
vrrpd_ds_t *vrrp_adv_check(uint8_t *buf, const int buflen, const char* intf, struct sockaddr *from);
void vrrp_process_incoming_adv(vrrpd_ds_t *vri_curr, vrrpd_ds_t *vri_rcv);

vrrp_ret_t vrrp_create_inst(vrrp_inst_t *inst, vrrpd_ds_t **vim);
vrrp_ret_t vrrpd_setup_netfd(vrrpd_ds_t *v);
vrrp_ret_t inst_start_recv(vrrpd_ds_t *vi);

int poll_add(int fd, int af, const char *intf);

//vrrp_ret_t vrrp_send_adv(vrrp_vr_t *vr, vrrp_attr_t *va, vrrp_intf_fd_t *netfd);

int main(int argc, char* argv[]){


	

	pthread_t pthid1,pthid2;
	
	
	/* create a daemon */	
	make_daemon();

	/* Unix domain socket to communicate with vrrpadm. */
	

	
	
	//timer thread for handling advertisements
	pthread_create(&pthid1, NULL, timer_thread, NULL);
	//pthread_create(&pthid2, NULL, timer_thread, NULL);
	
	//pthread_join(pthid2, NULL);
	/*infinitely accept commands from vrrpadm and process it*/
	/*for(;;)
	process_adm_sock(admsock);*/

	vrrpd_adm_thread();

	pthread_join(pthid1, NULL);
	return 0;
}

void vrrpd_adm_thread(){

	int admsock,vrrpsock;
	/*admsock: to communicate with vrrpadm
	vrrpsock: to send and receive VRRP advertisements*/
	int i;

	bzero(netfds, sizeof(netfds));

	admsock = setup_adm_socket();

	for (;;) {
		if (poll(pollfds, pollfd_num, -1) < 0) {
			if (errno == EINTR)
				continue;
			vrrpd_log("main(): poll(): %s\n", strerror(errno));
			exit(1);
		}
		for (i = 0; i < pollfd_num; i++) {
			if (!(pollfds[i].revents & POLLIN))
				continue;

			if (pollfds[i].fd == admsock) {
				process_adm_sock(admsock);
				break;
			} else {
				process_vrrpd_sock(pollfds[i].fd, poll_afs[i],poll_intfs[i]);
				break;
			}
		}
	}
	
	return;
}
int poll_add(int fd, int af, const char *intf){

	int i;
	int new_num;
	struct pollfd *newfds;

	/* Check if already present */
	for (i = 0; i < pollfd_num; i++) {
		if (pollfds[i].fd == fd)
			return (0);
	}

	/* Check for empty spot already present */
	for (i = 0; i < pollfd_num; i++) {
		if (pollfds[i].fd == -1) {
			pollfds[i].fd = fd;
			poll_afs[i] = af;
			if (intf != NULL)
				strcpy(poll_intfs[i], intf);
			return (0);
		}
	}

	/* Allocate space for 32 more fds and initialize to -1 */
	new_num = pollfd_num + 32;
	newfds = realloc(pollfds, new_num * sizeof (struct pollfd));
	if (newfds == NULL) {
		vrrpd_log("poll_add: realloc(): %s\n", strerror(errno));
                return (-1);
        }

	
	if (intf != NULL)
		strcpy(poll_intfs[pollfd_num], intf);
        newfds[pollfd_num].fd = fd;
        newfds[pollfd_num++].events = POLLIN;

        for (i = pollfd_num; i < new_num; i++) {
                newfds[i].fd = -1;
                newfds[i].events = POLLIN;
        }
        pollfd_num = new_num;
        pollfds = newfds;
        return (0);
}

void SIGCHLD_handler(int sig){

	vrrpd_log("received SIGCHLD signal:\n");
}
int make_daemon(){

	pid_t	pid;
	if ((pid = fork()) < 0)
		return (-1);
	if (pid != 0){
		/* in parent process: do nothing. */
		exit(0);
	}
	/*
	 * child process becames a daemon, return to main() to continue.
	 */
	setsid();
	chdir("/");
	umask(0);

	//signal(SIGCHLD, SIGCHLD_handler);
	return 0;
}

int setup_adm_socket(){

	int sock;
	int ret;
	int len;
	struct sockaddr_un vrrpd_adr;

	if ((sock = socket(AF_UNIX, SOCK_DGRAM, 0)) < 0) {
		
		vrrpd_log("setup_socket: socket(AF_UNIX): %s\n",strerror(errno));
                exit(1);
	}

	bzero(&vrrpd_adr, sizeof(vrrpd_adr));
	vrrpd_adr.sun_family = AF_UNIX;
	strncpy(vrrpd_adr.sun_path, VRRPD_VRRPADM_SOCKET, sizeof(vrrpd_adr.sun_path));
        len = sizeof(struct sockaddr_un);

	/*remove the existing socket with same name*/
	unlink(VRRPD_VRRPADM_SOCKET);

	if ((ret = bind(sock, (struct sockaddr *)&vrrpd_adr, len)) < 0) {

		vrrpd_log("setup_socket: bind(): %s\n", strerror(errno));
                exit(1);
	}

	/*if ((ret = fcntl(sock, F_SETFL, O_NONBLOCK)) < 0) {
		
		vrrpd_log("setup_socket: fcntl(): %s\n", strerror(errno));
		exit(1);
	}*/

	 if (poll_add(sock, AF_UNSPEC, NULL) == -1) {
                exit(1);
        }
	
	return sock;
}

void process_adm_sock(int sock){

	uint8_t	buf[BUFSIZE+1];
	uint8_t *p=buf;
	
	vrrp_cmd_t cmd;

	struct sockaddr_un vrrpadm_adr;

	vrid_t vid;		/* for many CMD_xxxx_INST commands */
	char intf[32];
	int af;
	char *ifname = intf;

	int pri;		/* for CMD_MODIFY_INST */
	int delay;
	int pree_mode;
	int acpt_mode;

	vrrp_ret_t ret = VRRP_EVRIP;	/*set to invalid virtual router IP*/

	vrrp_inst_t inst;	/* for CMD_GET_INSTX */
	vrrp_vr_t *vrp;

	vrrpd_ds_t *implp = NULL;	/* for CMD_CREATE_INST and CMD_GET_INSTX */
	
	int len;

	size_t vrrpadm_adr_len = (socklen_t)sizeof(struct sockaddr_un);

	
	len = recvfrom(sock, buf, sizeof (buf), 0, (struct sockaddr *)&vrrpadm_adr,&vrrpadm_adr_len);

	if (len < sizeof (vrrp_cmd_t)) {
                vrrpd_log("process_adm_sock(): bad VRRP command \n");
                return;
        }
	
	get_vrrp_cmd(buf, &cmd);

	uint8_t *ptr=buf;

	ptr+=sizeof(vrrp_cmd_t);
	
	/*vrid_t vid=0;
	char ifname[32];
	int af;

	memcpy((int*)&vid,(int*)ptr,sizeof(int));
	printf("vid= %d\n",vid);
	ptr+=sizeof(vrid_t);
	
	strcpy((char*)ifname,ptr);	
	printf("ifname: %s\n", ifname);
	ptr+=strlen(ifname)+1;
	
	memcpy((int*)&af, (int*)ptr, sizeof(int));
	printf("address family = %d\n",af);*/

	switch(cmd){

		case CMD_CREATE_INST:
					printf("RECEIVED \"CREATE INSTANCE COMMAND \"\n");

					vrrp_inst_t *vi;

					int idm;
					
					parse_vrrp_cmd(buf, cmd, &vi);

					vrrpd_ds_t *temp;
					ret = vrrp_create_inst(vi, &temp);

					if(ret!=VRRP_SUCCESS)
						break;

					//setup network fd for each instance
					vrrpd_setup_netfd(temp);

					inst_start_recv(temp);
					if(vi -> vi_va.pri == 255){

						ret = vrrp_state_i2m(temp);
						alarm(vi->vi_va.delay);
					}

					//else
					//	ret = vrrp_startup_inst(vi->vi_vr.vr_id, vi->vi_vr.vr_ifname, vi->vi_vr.vr_af);

					break;
		case CMD_REMOVE_INST:
					printf("RECEIVED \"REMOVE INSTANCE COMMAND \"\n");

					parse_vrrp_cmd(buf, cmd, &vid, intf, &af);
			
					if (intf[0] == '\0')
						ifname = NULL;	
				
					ret = vrrp_destroy_inst(vid, ifname, af);

					break;
		case CMD_STARTUP_INST:
					printf("RECEIVED \"STARTUP INSTANCE COMMAND \"\n");
		
					parse_vrrp_cmd(buf, cmd, &vid, intf, &af);

					if (intf[0] == '\0')
						ifname = NULL;

					ret = vrrp_startup_inst(vid, ifname, af);

					break;
		case CMD_SHUTDOWN_INST:
					printf("RECEIVED \"SHUTDOWN INSTANCE COMMAND \"\n");

					parse_vrrp_cmd(buf, cmd, &vid, intf, &af);

					if (intf[0] == '\0')
						ifname = NULL;

					ret = vrrp_shutdown_inst(vid, ifname, af);
			
					break;
		case CMD_MODIFY_INST:
					printf("RECEIVED \"MODIFY INSTANCE ATTRIBUTES COMMAND \"\n");

					parse_vrrp_cmd(buf, cmd, &vid, intf, &af, &pri, &delay,&pree_mode, &acpt_mode);

					if (intf[0] == '\0')
						ifname = NULL;
				
					ret = vrrp_set_inst_prop(vid, ifname, af, pri, delay,pree_mode, acpt_mode);

 					break;
		case CMD_GET_INSTX:
					printf("RECEIVED \"SHOW INSTANCE ATTRIBUTES COMMAND \"\n");

					parse_vrrp_cmd(buf, cmd, &vid, intf, &af);

					ret = vrrp_get_instx(&vid, intf, &af, &implp);

					break;
		case CMD_SHOW_ALL:
					printf("RECEIVED \"SHOW ALL INSTANCE ATTRIBUTES COMMAND \"\n");
					break;
		default:	
					break;
	}

	/*send reply indicating succeful exec of command*/
		
	p += build_vrrp_ret(buf, ret);
	
	if (ret == VRRP_SUCCESS)
		if (cmd == CMD_GET_INSTX)
			p += build_vrrp_ret_get_instx(p, implp, vid, intf, af);
	
	
	//vrrp_cmd_t tmp=CMD_RETURN_VAL;
	//memcpy(buf, &tmp, sizeof(vrrp_cmd_t));
	
	vrrpadm_adr_len = sizeof(vrrpadm_adr.sun_family) + strlen(vrrpadm_adr.sun_path);
	
	len = sendto(sock, buf, sizeof(p - buf), 0, (struct sockaddr *)&vrrpadm_adr, vrrpadm_adr_len);

	vrrpd_log("sent %d bytes\n", len);

	vrrpd_log("Current status:\n");

	//if(inst_runtime_list.next != NULL)
		display_vrrp_instx(&(inst_runtime_list));

	//close(sock);

	//unlink(VRRPD_VRRPADM_SOCKET);
}

/*Fucntion to build the return buffer to VRRPADM : contains status of execution of a command*/
int build_vrrp_ret(uint8_t *buf, vrrp_ret_t rval){

	uint8_t *p = buf;
        vrrp_cmd_t cmd = CMD_RETURN_VAL;

        memcpy(p, &cmd, sizeof (vrrp_cmd_t));
        p += sizeof (vrrp_cmd_t);
        memcpy(p, &rval, sizeof (vrrp_ret_t));

        return (sizeof (vrrp_cmd_t) + sizeof (vrrp_ret_t));
}

/*build return value for CMD_GET_INSTX which is an instance*/
int build_vrrp_ret_get_instx(uint8_t *buf, vrrpd_ds_t *v, vrid_t next_vid, const char *next_intf, int next_af){

	uint8_t *p = buf;

        memcpy(p, v, sizeof (vrrpd_ds_t));
        p += sizeof (vrrpd_ds_t);

        
        p += build_vrrp_vr_buf(p, v->vr, 1);

        memcpy(p, &next_vid, sizeof (vrid_t));
        p += sizeof (vrid_t);

        strcpy((char *)p, next_intf);
        p += strlen(next_intf) + 1;

        memcpy(p, &next_af, sizeof (int));
        p += sizeof (int);

        return ((size_t)(p - buf));
}

/*copy all the VR's in that instance into buffer and return the buffer to build_vrrp_ret_get_instx*/
int build_vrrp_vr_buf(uint8_t *buf, vrrpd_vr_ds_t *v, int num){
	
        uint8_t *p = buf;
        size_t len;
        vrrp_vr_t       *r;
        vrrpd_vr_ds_t* curr = v;
        int     i;

        for (i = 0; i < num; i++){
                memcpy(p, curr, sizeof (vrrpd_vr_ds_t));
                p += sizeof (vrrpd_vr_ds_t);

                r = &(curr->vr);
                len = r->vr_ipnum * sizeof(in_addr_t);
                memcpy(p, r->vr_ip, len);
                p += len;

                curr = curr->next;
        }
        
	return ((size_t)(p - buf));
}

/*Listen to the VRRPD socket for advertisements*/
void process_vrrpd_sock(int s, int af, const char* ifname){

	/*struct sockaddr_in from; */
	struct sockaddr from;
	uint8_t buf[4096];
	int buflen = sizeof (buf);
	VRRP_PKT *vp;
	//vrrpd_vr_ds_t *vri;
	vrrpd_ds_t *vri_rcv, *vri_curr;

	bzero(buf, 4096);
	/*receive IPv4 advertisement packets*/
	vrrp_recv_adv4(s, buf, &buflen);
	
	//if ((vp = vrrp_adv_check(buf, buflen, ifname, &from)) == NULL)
	if ((vri_rcv = vrrp_adv_check(buf, buflen, ifname, &from)) == NULL)
		return;

	/*search for VR with the given attributes in the received advertisement packet*/
	//vri = (vrrpd_vr_ds_t*)vrrp_search_vr((int)vp->vrid, (char*)ifname, (int)af);
	vrid_t vid_rcv = vri_rcv->vr->vr.vr_id;
	vri_curr = (vrrpd_ds_t*)vrrp_search_inst(vid_rcv, (char*)ifname,(int)af);

	//if (((int)vp->vrid == 0) || (vri == NULL))
	if ((vid_rcv == 0) || (vri_curr == NULL))  
	{
		return;
	}
	
	
	//vrrp_process_incoming_adv(vri, vp);
	vrrp_process_incoming_adv(vri_curr, vri_rcv);
}

/*This function receives the advertisement packet from socket sock& stores in buffer buf*/
int vrrp_recv_adv4(int sock, uint8_t *buf, int *buflen)
{
	struct sockaddr_in from4;
	struct sockaddr *from = (struct sockaddr *)&from4;
	int fromlen = sizeof (struct sockaddr_in);
	char str[INET_ADDRSTRLEN];
	int len = 0;

	len = recvfrom(sock, (void *)buf, *buflen, 0, from, &fromlen);
	*buflen = len;
	
	//printf("received a packet on vrrpd socket\n");

	
	return (len);
}

/*This function checks advertisement packets for errors viz, verifies checksum*/
//VRRP_PKT *vrrp_adv_check(uint8_t *buf, const int buflen, const char* intf, struct sockaddr *from){
vrrpd_ds_t *vrrp_adv_check(uint8_t *buf, const int buflen, const char* intf, struct sockaddr *from){

	struct iphdr *ip;
	
	int plen;		/* ip payload length */
	
	//VRRP_PKT *vp;
	int af;
	
	//vrrpd_vr_ds_t *vr;
	
	/* IP header check: protocol must be VRRP, TTL must be 255. */ 
	ip = (struct iphdr *)(buf+sizeof(struct ethhdr));
	
	//IPv4
	if(ip->version != 4) return NULL;
		
		//printf("received IPv4 packets\n\n");

		if(ip->protocol != IPPROTO_VRRP){

			//vrrpd_log("not vrrp 4 packet.\n");
			return NULL;
		}
		if (ip->ttl != VRRP_IP_TTL) {
			//vrrpd_log("invalid ttl %d; expecting %d.\n", ip->ttl, VRRP_IP_TTL);
			return NULL;
		}
		
		af = AF_INET;

		/*vp = (VRRP_PKT *)((unsigned char *)buf + ip->ihl * 4);
		plen = ntohs(ip->tot_len) - ip->ihl * 4;*/
	

	/*if (ComputeChecksum((unsigned char *)vp, plen) == 0) {
		vrrpd_log("vrrp packet checksum error.\n");
		return NULL;
	}*/
	//vrrpd_log("vrrp checksum ok.\n");
	
	/* verify: auth type match auth data. NO NEED */
	/*if (vp->auth_type != VRRP_AUTH_NONE) {
		vrrpd_log("vrrp packet auth type error.\n");
		return NULL;
	}*/

	//extract instance from the buffer and insert into runtime list
	vrrp_inst_t *inst;
	
	inst = (vrrp_inst_t*)(buf+sizeof(struct ethhdr)+sizeof(struct iphdr));

	flag[inst->vi_vr.vr_id]=1;

	//if priority is 0 then MASTER is going down... change the current BACKUP to MASTER
	if(inst->vi_va.pri == 0){
		vrrpd_ds_t *v_i=((&inst_runtime_list)->next);
		
		flag[inst->vi_vr.vr_id]=0;
		vrrpd_log("\n\nMASTER ROUTER WITH ID %d GOING DOWN!!!\n\n", inst->vi_vr.vr_id);
		if(v_i->state == VRRP_STATE_BACK){
			vrrpd_log("\n\nBACKUP ROUTER WITH ID %d COMMING UP -- BECOMING MASTER!!\n\n", v_i->vr->vr.vr_id);
			vrrp_state_b2m(v_i);
		}
		else
			vrrpd_log("\n\nNO INSTANCE RUNNING IN BACKUP STATE!!\n\n");
	
		return NULL;
	}
	/*printf("Entering following INSTNACE into runtime list\n\n");

	//printf("vrid: %d\n",vri->vr->vr.vr_id);

	printf("STATE: %s\n", vrrp_stats[vri->state]);

	printf("PRIORITY: %d\nDELAY:%d\n",vri->vr->attr.pri, vri->vr->attr.delay);
	
	printf("IFNAME: %s\n", vri->vr->vr.vr_ifname);

	vrrpd_ds_t *temp;
	
	vrrp_inst_t inst;
	inst.vi_state = vri->state;
	inst.vi_va.pri = vri->vr->attr.pri;
	inst.vi_va.pree_mode = vri->vr->attr.pree_mode;
	inst.vi_va.accept_mode = vri->vr->attr.accept_mode;
	inst.vi_vr.vr_id = vri->vr->vr.vr_id; 
	inst.vi_vr.vr_af = vri->vr->vr.vr_af;
	memcpy(&(inst.vi_vr.in4), &(vri->vr->vr.in4), sizeof(in_addr_t)); 
	inst.vi_vr.vr_ipnum = vri->vr->vr.vr_ipnum; 
	strcpy(inst.vi_vr.vr_ifname, vri->vr->vr.vr_ifname);
	int ret = vrrp_create_inst(&inst, &temp);

	return vri;*/

	vrrpd_vr_ds_t *vr;
	vr = (vrrpd_vr_ds_t*)malloc(sizeof(vrrpd_vr_ds_t));
	vrrpd_ds_t *vi;
	vi = (vrrpd_ds_t*)malloc(sizeof(vrrpd_ds_t));
	vi->vr = vr;

	vi->vr->attr.pri = inst->vi_va.pri;
	vi->vr->attr.delay = inst->vi_va.delay;
	vi->vr->attr.pree_mode = inst->vi_va.pree_mode;
	vi->vr->attr.accept_mode = inst->vi_va.accept_mode;

	vi->vr->vr.vr_id = inst->vi_vr.vr_id;
	vi->vr->vr.vr_af = inst->vi_vr.vr_af;
	memcpy(&(vi->vr->vr.in4), &(inst->vi_vr.in4), sizeof(in_addr_t));
	
	strcpy(vi->vr->vr.vr_ifname, inst->vi_vr.vr_ifname);
	
	vi->state = inst->vi_state;
	
	vi->active = inst->vi_active;
	
	vrrpd_ds_t *temp;
	inst->vi_vr.vr_ipnum = 0;
	//int ret = vrrp_create_inst(inst, &temp);
	return vi;
	
}

/*processes the received advertisement packets*/
//void vrrp_process_incoming_adv(vrrpd_vr_ds_t *vri, VRRP_PKT *vp){
void vrrp_process_incoming_adv(vrrpd_ds_t *vri_curr, vrrpd_ds_t *vri_rcv){

	/*if (vri->parent->state == VRRP_STATE_BACK){
		if (vri->attr.pree_mode == FALSE || vp->priority >= vri->attr.pri) {
				pthread_mutex_lock(&adv_q_lock);
				if (vrrp_rmqueue_impl(vri) == VRRP_SUCCESS){
					// Skew time 
					vri->timeout = VRRP_SEC2USEC((256 - vri->attr.pri)/256);
					if (vp->priority != 0)
						vri->timeout += 3 * VRRP_SEC2USEC(vri->attr.delay);
					vrrp_enqueue_impl(vri);
				}
				pthread_mutex_unlock(&adv_q_lock);
		}
	}else if (vri->parent->state == VRRP_STATE_MAST){
		if (vp->priority == 0) {
			//send an adv 
			pthread_mutex_lock(&adv_q_lock);
			vrrp_rmqueue_impl(vri);
			vri->timeout = VRRP_SEC2USEC(vri->attr.delay);
			vrrp_enqueue_impl(vri);
			pthread_mutex_unlock(&adv_q_lock);
		} else if (vp->priority > vri->attr.pri)
			//also when equaling 
			vrrp_state_m2b(vri->parent);

	} else 	printf("receive adv: vr in error state\n");

	return;*/
	
	vrrpd_vr_ds_t *vri = (vrrpd_vr_ds_t*)vri_curr->vr;

	vrrpd_log("\n\nINSTANCE WITH ID %d in %s STATE CAME ONLINE\n\n",vri_curr->vr->vr.vr_id ,vrrp_stats[vri_curr->state]);

	if (vri->parent->state == VRRP_STATE_BACK){
		if (vri->attr.pree_mode == FALSE || vri_rcv->vr->attr.pri >= vri->attr.pri) {
				pthread_mutex_lock(&adv_q_lock);
				if (vrrp_rmqueue_impl(vri) == VRRP_SUCCESS){
					// Skew time 
					vri->timeout = VRRP_SEC2USEC((256 - vri->attr.pri)/256);
					if (vri_rcv->vr->attr.pri != 0)
						vri->timeout += 3 * VRRP_SEC2USEC(vri->attr.delay);
					vrrp_enqueue_impl(vri);
				}
				pthread_mutex_unlock(&adv_q_lock);
		}
	}else if (vri->parent->state == VRRP_STATE_MAST){
		if (vri_rcv->vr->attr.pri == 0) {
			//send an adv 
			pthread_mutex_lock(&adv_q_lock);
			vrrp_rmqueue_impl(vri);
			vri->timeout = VRRP_SEC2USEC(vri->attr.delay);
			vrrp_enqueue_impl(vri);
			pthread_mutex_unlock(&adv_q_lock);
		} else if(vri_rcv->vr->attr.pri > vri->attr.pri)
			//also when equaling 
			vrrp_state_m2b(vri->parent);

	} else 	printf("receive adv: vr in error state\n");

	return;

}


/*
 * i: index of netfds[]. will be initialized.
 * intf: which interface.
 *
 * returns: 0: success; -1: failure.
 *
 * will initial netfds[i] vrrp_intf_fd_t object;
 * create its sockets and dlpi handles: dh, s4, s6.
 * But s4 and s6 don't join the multicast group for now; they'll join
 * when they become backup (in state_goto_backup()).
 */
int init_netfd(int i, const char *intf, int af){

	/*
	 * When called for the first time, create sockets in raw mode
	 * s_send will be used for sending IPv4 adv.
	 * s_rcv will be used for receiving IPv4 adv
	 */
	//if (netfds[i].intf[0] == '\0') {
		strcpy(netfds[i].intf, intf);
		netfds[i].s_send = 0;
		netfds[i].s_rcv = 0;

	/*create a raw socket for sending adv*/		
	netfds[i].s_send = CreateSock(ETH_P_ALL);	
	
	/*create a raw socket for receiving adv*/		
	netfds[i].s_rcv = CreateSock(ETH_P_IP);	

	/*Bind the created raw sockets*/
	BindSock(netfds[i].intf, netfds[i].s_send, ETH_P_ALL);
	BindSock(netfds[i].intf, netfds[i].s_rcv, ETH_P_IP);

	/*join multicast group*/
	JoinMulticast(INADDR_VRRP_GROUP, netfds[i].s_send);		
	JoinMulticast(INADDR_VRRP_GROUP, netfds[i].s_rcv);
}
/*
 * return 0: success; -1: failure
 *
 * initializes the corresponding element in the global netfds[] array,
 *   if it's not initialized yet;
 * then set its addr to fp.
 * Finally: *fp point to the corresponding netfds[] element.
 */
int vrrpd_setup_netfd_vr(vrrp_intf_fd_t **fp, const char *intf, int af){

	int num = sizeof (netfds) / sizeof (vrrp_intf_fd_t);
	int n1 = num;	/* index of first empty netfds[] element */
	int i;
	for (i = 0; i < num; i++) {
		if (netfds[i].intf[0] == '\0') {
			if (i < n1)
				n1 = i;
		} else if (strcmp(netfds[i].intf, intf) == 0) {
			vrrpd_log("\tnetfd for %s found at [%d]\n", intf, i);
			*fp = &netfds[i];
			n1 = i;
			break;
		}
	}

	if (*fp == NULL)
		vrrpd_log("\tnetfd for %s to be inited at [%d]\n", intf, n1);

	if (init_netfd(n1, intf, af) < 0) {
		vrrpd_log("vrrpd_setup_netfd_vr() failed.\n");
		return (-1);
	}

	*fp = &netfds[n1];

	return (0);
}
/*
 * called after vrrp_create_inst() being called.
 * to initialize the vfd (array) in vrrpd_vr_ds_t
 */
/*setup network file descriptors for an instance*/
vrrp_ret_t vrrpd_setup_netfd(vrrpd_ds_t *v){

	/*For each VR in this instance setup netfds*/

	vrrpd_vr_ds_t *r;
	int i;
	
	for (r = v->vr, i = 0; r != NULL; r = r->next, i++)
		if (vrrpd_setup_netfd_vr(&(r->vfd), r->vr.vr_ifname, r->vr.vr_af) < 0){
		vrrpd_log("vrrpd_setup_netfd() failed.\n");
		return -1;
	}
	
	return 0;
}

/*function for and instance to receive advertisement message */
vrrp_ret_t inst_start_recv(vrrpd_ds_t *v){

	vrrpd_vr_ds_t	*r;
	vrrp_intf_fd_t	*fd;
	int	af;
	int	i;

	for (r = v->vr, i = 0; r != NULL; r = r->next, i++) {
		af = r->vr.vr_af;
		fd = r->vfd;
		if (poll_add(fd->s_rcv, af, fd->intf) < 0) {
			vrrpd_log("poll_add() failed.\n");
			return;
		}
	}

}

void
signal_alarm(int sig)
{
	/* send a packet */
	vrrpd_log("signal_alarm() called. in vrrpd_adm_thread\n");
	vrrp_send_adv_impl();
	signal(SIGALRM, signal_alarm);

	/* should use ualarm() or setitimer() instead. */
	alarm(2);
}

uint8_t *vrrp_vr_vmac(vrid_t vrid, int af){

	static uint8_t addr[6] = { 0x00, 0x00, 0x5e, 0x00, 0x01, 0x00 };
	addr[5] = vrid;
	uint8_t *vmac;
	vmac = (uint8_t*)malloc(sizeof(uint8_t)*6);
	memcpy(vmac, addr, 6);
	return (vmac);
}

/*VRRP HEADER LENGTH*/
//int vrrp_hdr_len(vrrp_vr_t *vr) {
int vrrp_hdr_len(vrrpd_ds_t *vr) {

	//return (sizeof (VRRP_PKT) + vr->vr_ipnum * sizeof(in_addr_t)+VRRP_AUTH_LEN);
	return (sizeof(vrrp_inst_t));
}

void vrrp_build_pkt(vrrpd_ds_t* p, unsigned char *buf, int buflen, int priority){

	/* build the ethernet header */
	//CreateEthernetHeader(vr, buf, buflen, ETH_P_IP);
	vrrp_vr_t *vr=&(p->vr->vr);
	CreateEthernetHeader(vr, buf, buflen, ETH_P_IP);
	buf += sizeof(struct ethhdr);
	buflen -= sizeof(struct ethhdr);

	/* build the ip header */
	//CreateIPHeader(vr, va, buf, buflen);
	struct in_addr in;
	memcpy(&(in.s_addr), &(vr->in4), sizeof(unsigned long));
	char *dstip;
	dstip = (char*)malloc(sizeof(char)*20);
	strcpy(dstip, (char*)inet_ntoa(in));

	//printf("SENDING: %s\n\n",dstip);
	//printf("STATE: %s\n\n",vrrp_stats[p->state]);
	CreateIPHeader(vr, buf, buflen);
	buf += sizeof(struct iphdr);
	buflen -= sizeof(struct iphdr);

	/* build the vrrp header */
	//CreateVRRPHeader(vr, va, buf, buflen);
	CreateVRRPHeader(p, buf, buflen, priority);
}

/*Function to send adv to master*/
//vrrp_ret_t vrrp_send_adv(vrrp_vr_t *vr, vrrp_attr_t *va, vrrp_intf_fd_t *netfd)
vrrp_ret_t vrrp_send_adv(vrrpd_ds_t *p, int priority){

	unsigned char buf[4096];
	int buflen;
	int ret;

	vrrp_intf_fd_t *netfd;

	netfd = p->vr->vfd;

	//size of adv packet
	buflen = sizeof(struct ethhdr) + sizeof(struct iphdr) + vrrp_hdr_len(p);

	//BUILD VRRP PACKET NOW
	//vrrp_build_pkt(vr, va, buf, buflen);
	vrrp_build_pkt(p, buf, buflen, priority);
	if(netfd == NULL)
		vrrpd_log("netfd NULL\n");
	
	if(netfd->s_send == -1)
		vrrpd_log("netfd sending socket NULL\n");
	

	if ((ret = SendVRRPPacket(netfd->s_send, buf, buflen)) == VRRP_SUCCESS)
		return VRRP_SUCCESS;	
	else
		return VRRP_ESYS;
}
