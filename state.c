/*
	state.c: monitors the state of each instance & also changes the state of the instance.....
*/
#include "header.h"
#include "vrrp.h"
#include "libvrrp.h"

#include "vrrpd_impl.h"

#include <sys/socket.h>



extern pthread_mutex_t adv_q_lock;
extern int flag[256];

vrrp_ret_t vrrp_state_i2b(vrrpd_ds_t *v);
vrrp_ret_t vrrp_state_b2i(vrrpd_ds_t* v);
vrrp_ret_t vrrp_state_i2m(vrrpd_ds_t *v);
vrrp_ret_t vrrp_state_b2m(vrrpd_ds_t *v);
vrrp_ret_t vrrp_master(vrrpd_vr_ds_t *vri);
vrrp_ret_t vrrp_unmaster(vrrpd_vr_ds_t *vri);
vrrp_ret_t vrrp_state_execution(vrrp_state_trans_t state, vrrpd_ds_t *v);
int any_master_up();


/*change state of an instance from INITIALIZE state to MASTER state*/
vrrp_ret_t vrrp_state_i2m(vrrpd_ds_t *v){

	vrrp_ret_t r,ret;
	vrrpd_vr_ds_t *vri;

	for(vri = v->vr; vri!=NULL; vri=vri->next)
		if((r=vrrp_master(vri)) != VRRP_SUCCESS)
			ret = r;
		 else{

	                 vri->timeout = VRRP_SEC2USEC(v->vrrp_delay);
                         pthread_mutex_lock(&adv_q_lock);
                         vrrp_enqueue_impl(vri);
                         pthread_mutex_unlock(&adv_q_lock);
                  }


	if(r == VRRP_SUCCESS){
		v->state = VRRP_STATE_MAST;
		vrrp_state_execution(INIT_TO_MAST, v);
		//vrrp_master(vri);
	}
	
	return ret;

}

/*change state of an instance from INITIALIZE state to BACKUP state*/
vrrp_ret_t vrrp_state_i2b(vrrpd_ds_t *v){

	vrrpd_vr_ds_t *vri;
	
	for(vri=v->vr; vri!=NULL;vri=vri->next){	
		vri->timeout = VRRP_SEC2USEC(v->vrrp_delay);
		pthread_mutex_lock(&adv_q_lock);
		vrrp_enqueue_impl(vri);
		vrrpd_log("Entered VR instance into ADV queue\n\n");
		pthread_mutex_unlock(&adv_q_lock);
	}

	v->state = VRRP_STATE_BACK;
	sleep(v->vr->attr.delay);

	if(any_master_up() == 0){
		
		vrrpd_log("\n\nNo Instance running in MASTER state - switching current BACKUP router to MASTER\n\n");	
		vrrp_state_b2m(v);
	}

	return VRRP_SUCCESS;
}

/*change state of an instance from MASTER state to INITIALIZE state*/
vrrp_ret_t vrrp_state_m2i(vrrpd_ds_t *v)
{
	vrrp_ret_t r, ret = VRRP_SUCCESS;
	vrrpd_vr_ds_t *vri;

	
		for (vri = v->vr; vri != NULL; vri = vri->next) {
			if ((r = vrrp_unmaster(vri)) != VRRP_SUCCESS)
				ret = r;
			else {
				
				pthread_mutex_lock(&adv_q_lock);
				vrrp_rmqueue_impl(vri);
				pthread_mutex_unlock(&adv_q_lock);
			}
		}
	

	if (ret == VRRP_SUCCESS) {
		v->state = VRRP_STATE_INIT;
		vrrp_state_execution(MAST_TO_INIT, v);
	}

	return ret;
}

/*change state of an instance from INITIALIZE state to BACKUP state*/
vrrp_ret_t vrrp_state_b2i(vrrpd_ds_t* v){

	vrrpd_vr_ds_t *vri;

	for(vri = v->vr; vri!=NULL; vri = vri->next){
		vri->timeout = VRRP_SEC2USEC(v->vrrp_delay);
		pthread_mutex_lock(&adv_q_lock);
		vrrp_rmqueue_impl(vri);
		pthread_mutex_unlock(&adv_q_lock);
	}

	v->state = VRRP_STATE_INIT;
	//vrrp_state_execution(BACK_TO_INIT, v);

	return VRRP_SUCCESS;
}

/*change state of an instance from BACKUP state to MASTER state*/
/* Called in queue mutex, no need to lock */
vrrp_ret_t vrrp_state_b2m(vrrpd_ds_t *v)
{
	vrrp_ret_t r, ret = VRRP_SUCCESS;
	vrrpd_vr_ds_t *vri;

	for (vri = v->vr; vri != NULL; vri = vri->next) {
		if ((r = vrrp_master(vri)) != VRRP_SUCCESS) {
			ret = r;
		} else {
			//pthread_mutex_lock(&adver_queue_lock);
			vrrp_rmqueue_impl(vri);
			vri->timeout = VRRP_SEC2USEC(v->vrrp_delay);
			vrrp_enqueue_impl(vri);
			//pthread_mutex_unlock(&adver_queue_lock);
		}
	}
	

	if (ret == VRRP_SUCCESS) {
		v->state = VRRP_STATE_MAST;
		vrrp_state_execution(BACK_TO_MAST, v);
		//vrrpd_log("\n\nSWITCHING FROM BACKUP TO MASTER STATE\n\n");
		alarm(v->vr->attr.delay);
		//vrrpd_log("TIMER INVOKED.......\n\n");
	}

	return ret;
}

/*change state of an instance from MASTER state to BACKUP state*/
vrrp_ret_t vrrp_state_m2b(vrrpd_ds_t *v)
{
	vrrp_ret_t r, ret = VRRP_SUCCESS;
	vrrpd_vr_ds_t *vri;

	
	for (vri = v->vr; vri != NULL; vri = vri->next)
		if ((r = vrrp_unmaster(vri)) != VRRP_SUCCESS)
			ret = r;
		else {
			pthread_mutex_lock(&adv_q_lock);
			vrrp_rmqueue_impl(vri);
			vri->timeout = VRRP_SEC2USEC(v->vrrp_delay);
			vrrp_enqueue_impl(vri);
			pthread_mutex_unlock(&adv_q_lock);
		}
		

	if (ret == VRRP_SUCCESS) {
		v->state = VRRP_STATE_BACK;
		vrrp_state_execution(MAST_TO_BACK, v);
	}
	sleep(v->vr->attr.delay);

	if(any_master_up() == 0){
		vrrpd_log("\n\nNo Instance running in MASTER state - switching current BACKUP router to MASTER\n\n");	
		vrrp_state_b2m(v);
	}

	return ret;
}

int any_master_up(){

	int i;

	for(i=1; i<=255; i++)
		if(flag[i] == 1)
			return i;
	
	return 0;
}
/*functios to be performed by instance in MASTER state*/
vrrp_ret_t vrrp_master(vrrpd_vr_ds_t *vri){



	return VRRP_SUCCESS;

}

vrrp_ret_t vrrp_unmaster(vrrpd_vr_ds_t *vri){

	return VRRP_SUCCESS;
}

vrrp_ret_t vrrp_state_execution(vrrp_state_trans_t state, vrrpd_ds_t *v){

	char *ifname = v->vr->vr.vr_ifname;
	uint8_t* vmac;
	vmac = (uint8_t*)malloc(sizeof(uint8_t)*6);
	memcpy(vmac,(uint8_t*)vrrp_vr_vmac(v->vr->vr.vr_id, AF_INET), 6);
	
	if((state == INIT_TO_MAST) || (state == BACK_TO_MAST)){
		pid_t pid;
		struct sigaction act;

		if ((pid = fork()) == -1) {
			vrrpd_log("vrrp run_execution: failed to fork");
			return (-1);
		}
	
		if (pid == 0) {
			act.sa_handler = SIG_IGN;
			(void) sigaction(SIGALRM, &act, NULL);
			(void) sigaction(SIGUSR1, &act, NULL);
			//(void) execl("/tmp/router", "eth0", "eth0","eth0",(char*)0);
			struct ether_addr *ethaddr;

			ethaddr = (struct ether_addr*)malloc(sizeof(uint8_t)*6);
			memcpy(ethaddr, vmac, 6);	
			char *eth = (char*)ether_ntoa(ethaddr);
			
			(void) execl("/tmp/router", ifname, ifname,ifname, eth, eth, eth, NULL);
	
			exit(127);
		}
	
		//vrrpd_log("pid: %d\n", pid);
		v->pid = pid;
	
		//if (waitpid(pid, NULL, 0) != pid)
		//	vrrpd_log("waitpid error\n");
		return (0);
	}

	else if((state == MAST_TO_BACK) || (state == MAST_TO_INIT)){

	//send an advertisement indicating that master is going down... so that other backup router can become master
		vrrp_send_adv(v, 0);
		int ret = kill(v->pid, SIGINT);
	
		if(ret == 0)
			return VRRP_SUCCESS;
	
		return -1;
	}
}

