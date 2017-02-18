/*
	libvrrp.c: shared library
*/
#include "header.h"

#include "vrrp.h"

#include "libvrrp.h"

char* af_str(int af){

	if (af == 4 || af == AF_INET)
		return ("AF_INET");
	else if (af == 6 || af == AF_INET6)
		return ("AF_INET6");
	else if (af == AF_UNSPEC)
		return ("AF_UNSPEC");
	else
		return ("AF_error");
}

int get_vrrp_cmd(uint8_t *buf, vrrp_cmd_t *cmd){

	memcpy(cmd, buf, sizeof (vrrp_cmd_t));
	return 0;
}

int create_vrrpd_socket(struct sockaddr_un *to){

	int sock;
	if((sock = socket(AF_UNIX, SOCK_DGRAM, 0)) < 0){
	
		printf("socket(): %s\n",strerror(errno));
		return -1;
	}
	
	struct sockaddr_un vrrpd_adr;
	
	unlink(VRRPADM_VRRPD_SOCKET);
	vrrpd_adr.sun_family = AF_UNIX;
	strcpy(vrrpd_adr.sun_path,VRRPADM_VRRPD_SOCKET);
		
	if(bind(sock, (struct sockaddr*)&vrrpd_adr, sizeof(struct sockaddr_un)) < 0){

		printf("bind(): %s\n", strerror(errno));
		return -1;
	}
	
	to->sun_family=AF_UNIX;
	strcpy(to->sun_path, VRRPD_VRRPADM_SOCKET);
	
	return sock;
	
}
	
/*
 *general function that accepts variable no. of args finds command length
 *and sends the command to vrrpd using function issue_vrrp_cmd()
 */
vrrp_ret_t vrrp_all_cmd(vrrp_cmd_t cmd, ...){

	uint8_t buf[BUFSIZE+1];

	va_list ap;
	va_start(ap, cmd);

	int cmd_len=build_vrrp_cmd(buf, cmd, ap);
	
	va_end(ap);
	
	vrrp_ret_t ret=issue_vrrp_cmd(buf, &cmd_len);

	if(ret != -1)
		return VRRP_SUCCESS;
}

/*send the command to vrrpd*/
int issue_vrrp_cmd(uint8_t *buf, size_t *plen){

	struct sockaddr_un to,from;
		
	int sock=create_vrrpd_socket(&to);
	
	int len = sendto(sock, buf, *plen, 0, (struct sockaddr*)&to, sizeof(struct sockaddr_un));

	printf("SENT %d of %d bytes\n",len, *plen);
	
	if(len<0){
	
		printf("sendto(): error in sending command to vrrpd\n");
		return -1;
	}
	
	int from_len=sizeof(struct sockaddr_un);

	//sleep(5);

	size_t cmd_len = 65535;
	from.sun_family = AF_UNIX;
	strcpy(from.sun_path, VRRPD_VRRPADM_SOCKET);
	
	from_len = sizeof(from.sun_family) + strlen(from.sun_path);

	*plen = recvfrom(sock, buf, cmd_len, 0, (struct sockaddr*)0, (int*)0);

	vrrp_cmd_t cmd;
	vrrp_ret_t *ret = ((vrrp_ret_t*)buf) + sizeof(vrrp_cmd_t);
	get_vrrp_cmd(buf, &cmd);
	
	if(cmd != CMD_RETURN_VAL){
		
		printf("INVALID RETURN VALUE\n");
		return -1;
	}
	else printf("RECEIVED CMD_RETURN_VAL command from vrrpd\n");

	if(*plen == (sizeof(vrrp_cmd_t) + sizeof(vrrp_ret_t)))
		printf("RETURN VALUE: %s\n",vrrp_rets[*ret]);
	
	close(sock);
	unlink(VRRPADM_VRRPD_SOCKET);
	return (*plen);
}

/*builds the vrrp command and finds its length*/
int build_vrrp_cmd(uint8_t *buf, vrrp_cmd_t cmd, va_list alist){

	uint8_t *ptr = buf;
	int pri, delay, pree_mode, acpt_mode;
	vrid_t vid;	
	char *intf;
	int af;
	int alen;
	
	vrrp_inst_t *vi;
		
	memcpy((void*)ptr, (const void*)&cmd, sizeof(vrrp_cmd_t));

	ptr+=sizeof(vrrp_cmd_t);

	if(cmd == CMD_REMOVE_INST || cmd == CMD_STARTUP_INST || cmd == CMD_SHUTDOWN_INST || cmd == CMD_MODIFY_INST || cmd == CMD_GET_INSTX){

		vid = va_arg(alist, int);
		intf = va_arg(alist, char*);
		af = va_arg(alist, int);

		printf("vid: %d\nintf:%s\naf:%d\n",vid, intf, af);

		memcpy(ptr, &vid, sizeof(vrid_t));
		ptr+= sizeof(vrid_t);

		strcpy((char*) ptr, intf);
		ptr+=strlen(intf)+1;

		memcpy(ptr, &af, sizeof(int));
		ptr+=sizeof(int);

	}

	switch(cmd){

		case CMD_CREATE_INST:
			vi = va_arg(alist, vrrp_inst_t *);
	
			struct in_addr in;
			memcpy(&(in.s_addr), &(vi->vi_vr.in4), sizeof(unsigned long));
			char *dstip;
			dstip = (char*)malloc(sizeof(char)*20);
			strcpy(dstip, (char*)inet_ntoa(in));

			printf("PIP: %s\n",dstip);
		
			memcpy(ptr, vi, sizeof (vrrp_inst_t));
			ptr += sizeof (vrrp_inst_t);

			alen = vi->vi_vr.vr_ipnum * sizeof(in_addr_t);
			memcpy(ptr, vi->vi_vr.vr_ip, alen);
			ptr += alen;
			
			break;
		case CMD_MODIFY_INST:
			pri = va_arg(alist, int);
			delay = va_arg(alist, int);
			pree_mode = va_arg(alist, int);
			acpt_mode = va_arg(alist, int);

			memcpy(ptr, &pri, sizeof(int));
			ptr+=sizeof(int);

			memcpy(ptr, &delay, sizeof(int));
			ptr+=sizeof(int);

			memcpy(ptr, &pree_mode, sizeof(int));
			ptr+=sizeof(int);

			memcpy(ptr, &acpt_mode, sizeof(int));
			ptr+=sizeof(int);
			
			break;

		default:
			break;
	}
	
	va_end(alist);
	
	return ((size_t)(ptr-buf));
}

/*public API's*/

//cmd create - create an instance
vrrp_ret_t cmd_create_inst(vrrp_inst_t *inst){

	return (vrrp_all_cmd(CMD_CREATE_INST, inst));
}

//cmd destroy - remove an instance
vrrp_ret_t cmd_destroy_inst(vrid_t vid, char* ifname, int af){

	return (vrrp_all_cmd(CMD_REMOVE_INST, (vrid_t)vid, ifname, af));
}

//cmd startup - send startup signal to an instance
vrrp_ret_t cmd_startup_inst(vrid_t vid, char* ifname, int af){

	return (vrrp_all_cmd(CMD_STARTUP_INST, (vrid_t)vid, ifname, af));
}

//cmd shutdown - send shutdown signal to an instance
vrrp_ret_t cmd_shutdown_inst(vrid_t vid, char* ifname, int af){

	return (vrrp_all_cmd(CMD_SHUTDOWN_INST, (vrid_t)vid, ifname, af));
}

//cmd set - set instance properties
vrrp_ret_t cmd_set_inst_prop(vrid_t vid, char* ifname, int af, int prio, int delay, int pree_mode, int acpt_mode){
	
	return (vrrp_all_cmd(CMD_MODIFY_INST, (vrid_t)vid, ifname, af, prio, delay, pree_mode, acpt_mode));
}

/*vrrp_ret_t cmd_show_inst(vrid_t vid, char *ifname, int af){

	return (vrrp_all_cmd(CMD_GET_INSTX, (vrid_t)vid, ifname, af));
}*/

//cmd getx - show particular instance
vrrp_ret_t cmd_get_instx(vrid_t vid, char *ifname, int af, vrrpd_ds_t **im){
	
	return (vrrp_all_cmd(CMD_GET_INSTX, vid, ifname, af, im));
}

//cmd show - show all instances
vrrp_ret_t cmd_show_all(){

	vrrpd_ds_t *imp;
	vrid_t vid = 0;
	char ifname[LIFNAMSIZ] = { '\0' };
	int af = AF_UNSPEC;
	
	do {
		if (cmd_get_instx(vid, ifname, af, &imp)!=VRRP_SUCCESS)
			break;
		
		display_vrrp_instx(imp);
		free_vrrp(imp);
		vid++;
	} while ((vrid_t)vid < 256);
	
	return VRRP_SUCCESS;
}

char* vrrp_strerror(int err){

	return vrrp_rets[err];
}

/*vrrpd functions*/

/*parse buffer to obtain the parameters for that command and fill these parameters in suitabe variable*/
int parse_vrrp_cmd(uint8_t *buf, vrrp_cmd_t cmd, ...){


	vrrp_inst_t **vi;/*for CMD_CREATE_INST*/
	vrrp_ret_t *rvalp;	/* for CMD_RETURN_VAL */

	int *prio;		/* for CMD_MODIFY_INST */
	int *delay;
	int *pree_mode;
	int *acpt_mode;

	va_list alist;
	
	va_start(alist, cmd);

	uint8_t *ptr=buf;

	ptr += sizeof(vrrp_cmd_t);

	if (cmd == CMD_REMOVE_INST || cmd == CMD_STARTUP_INST || cmd == CMD_SHUTDOWN_INST || cmd == CMD_MODIFY_INST || cmd == CMD_GET_INSTX) {
		vrid_t *vid = va_arg(alist, vrid_t *);
                char *intf = va_arg(alist, char *);	/* char[] */
                int *af = va_arg(alist, int *);

                memcpy(vid, ptr, sizeof (vrid_t));
                ptr += sizeof (vrid_t);
                strcpy(intf, (char *)ptr);
                ptr += strlen(intf) + 1;
                memcpy(af, ptr, sizeof (int));
                ptr += sizeof (int);
	}
	
	switch(cmd){

		case CMD_CREATE_INST:
					vi = va_arg(alist, vrrp_inst_t**);
					*vi = (vrrp_inst_t*)ptr;
					ptr += sizeof(vrrp_inst_t);
					(*vi) -> vi_vr.vr_ip = ptr;

					break;
		
		case CMD_MODIFY_INST:

					prio = va_arg(alist, int *);
					delay = va_arg(alist, int *);
					pree_mode = va_arg(alist, int *);
					acpt_mode = va_arg(alist, int *);

					memcpy(prio, ptr, sizeof (int));
					ptr += sizeof (int);

					memcpy(delay, ptr, sizeof (int));
					ptr += sizeof (int);

					memcpy(pree_mode, ptr, sizeof (int));
					ptr += sizeof (int);

					memcpy(acpt_mode, ptr, sizeof (int));
					ptr += sizeof (int);
					break;

		case CMD_RETURN_VAL:
					rvalp = va_arg(alist, vrrp_ret_t *);
					memcpy((void *)rvalp, (const void*)ptr, sizeof(vrrp_ret_t));
					break;
			
		default:	
					break;

	}

	va_end(alist);
	return 0;
}

void display_vrrp_instx(vrrpd_ds_t *v){
	
	char s_id[8], s_stt[128];
	s_stt[0] = 0;
	int i;
	printf("\n--------------------------------------------\n\n");
	if(v == NULL) {printf("NO INSTANCES FOUND\n\n");return;}
	v = v->next;
	if(v == NULL) {printf("NO INSTANCES FOUND\n\n");return;}
	timeval_to_string(&v->state_trans_time, s_stt, sizeof (s_stt));
	if (v->vr == NULL) {printf("NO VIRTUAL ROUTERS FOUND\n\n");return;}
	sprintf(s_id, "%d", v->vr->vr.vr_id);
	printf("INSTANCE VRID: %s\n", s_id);	
	//printf("active: %s\n", BOOL(v->active));
        printf("state: %s\n", vrrp_stats[v->state]);
        //printf("prev_state: %s\n", vrrp_stats[v->previous_state]);
        printf("state_trans_time: %s\n", s_stt);

        vrrpd_vr_ds_t* curr = v->vr;
	while(curr!=NULL){
		dump_vrrp_vr(curr);
		curr = curr->next;
	}
	printf("\n--------------------------------------------\n\n");
}

int timeval_to_centi(struct timeval tv){
	return (tv.tv_sec * 100 + tv.tv_usec / 1000000 + 0.5);
}

struct timeval timeval_delta(struct timeval t1, struct timeval t2){

	struct timeval t = { t1.tv_sec - t2.tv_sec, t1.tv_usec - t2.tv_usec };
	if (t.tv_usec < 0) {
		t.tv_usec += 1000000;
		t.tv_sec--;
	}
	return (t);
}

char* timeval_to_string(struct timeval *tv, char *buf, size_t buflen){
	
	struct tm *t = localtime((time_t *)&(tv->tv_sec));
	sprintf(buf, "%04d.%02d.%02d %02d:%02d:%02d.%03d", t->tm_year + 1900, t->tm_mon + 1, t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec, (int)tv->tv_usec / 1000);

	return (buf);
}

void dump_vrrp_vr(vrrpd_vr_ds_t *v){


	char s_asrc[INET6_ADDRSTRLEN];
	char s_ats[128], s_apri[8], s_aitv[8], s_aage[16], s_skew[8], s_mdi[8], s_mdt[8];

	struct timeval now;
	int aage, skew, mdi;


	//if ((inet_ntop(v->ai_af, &v->advinfo.addr, s_asrc, sizeof (s_asrc))) == NULL)
	//	strcpy(s_asrc, "addr_unknown");

        timeval_to_string(&v->ai_timestamp, s_ats, sizeof (s_ats));
	sprintf(s_apri, "%d", v->ai_priority);
	sprintf(s_aitv, "%d", v->ai_interval);

	gettimeofday(&now, NULL);
	aage = timeval_to_centi(timeval_delta(now, v->ai_timestamp));
	skew = v->ai_interval * (256 - v->attr.pri) / 256;
	mdi = v->ai_interval * 3 + skew;
	sprintf(s_aage, "%d", aage);
	sprintf(s_skew, "%d", skew);
	sprintf(s_mdi, "%d", mdi);
	sprintf(s_mdt, "%d", mdi - aage);

	dump_vr(&v->vr);
	
	dump_vrrp_attr(&v->attr);	

	
	/*printf("adv_addr_family: %s\n", af_str(v->ai_af));
	printf("adv_src_ipaddr: %s\n", s_asrc);
	printf("adv_timestamp: %s\n", s_ats);
	printf("adv_age: %s\n", s_aage);
	printf("adv_interval: %s\n", s_aitv );
	printf("skew_time", s_skew);
	printf("master_down_interval: %s\n", s_mdi);
	printf("master_down_timer: %s\n", s_mdt);*/
	
}

void dump_vr(vrrp_vr_t *vr){

	char ap[INET6_ADDRSTRLEN];
	in_addr_t *p4;
	int i;
	char s_vrid[8], s_ipnum[8], s_ip[256] = {0};
	
	p4 = (in_addr_t *)vr->vr_ip;
	
	for (i = 0; i < vr->vr_ipnum; i++){
		strcat(s_ip, inet_ntop(vr->vr_af, (const void *)&p4[i], ap, sizeof (ap)));
		strcat(s_ip, ",");
	}
	
	s_ip[strlen(s_ip) - 1] = '\0';
	sprintf(s_vrid, "%d", vr->vr_id);
	printf("INSTANCE VRID: %s\n", s_vrid);
	printf("addr_family: %s\n", af_str(vr->vr_af));
	printf("ifname: %s\n", vr->vr_ifname);
	printf("vr_pip: %s\n", inet_ntop(vr->vr_af, (const void*)&vr->in4, ap, sizeof (ap)));
	//printf("ip_num: %s\n", s_ipnum);
	//printf("vr_pip: %s\n", s_ip);
}

void dump_vrrp_attr(vrrp_attr_t *va){

	char s_pri[32], s_delay[32];

	sprintf(s_pri, "%d", va->pri);
	sprintf(s_delay, "%d", va->delay);
	printf("priority: %s\n", s_pri);
	printf("adv_intval: %s\n", s_delay);
	printf("preempt_mode: %s\n", BOOL(va->pree_mode));
	printf("accept_mode: %s\n", BOOL(va->accept_mode));
}

void free_vrrp(vrrpd_ds_t *im){

	free(im->vr);
	free(im);
}
