/*
	timer: the advertisement timer that fires to trigger sending of adv in master state. In backup state the master
	down timer that fires when adv is not has not been received.....
*/

#include "header.h"
#include "libvrrp.h"
#include "vrrpd_impl.h"

static void signal_alarm(int);

void *timer_thread()
{
	//initialize and register signal handler
	signal(SIGALRM, signal_alarm);
	//alarm(1);
	while (1)
		(void) pause();
}

void signal_alarm(int sig)
{
	vrrpd_vr_ds_t *entry = NULL, *next_entry = NULL;
	vrrpd_ds_t *p;
	boolean_t more = TRUE;

	/* send a packet */
	//vrrpd_log("signal_alarm() called.\n");
	/* vrrp_send_adv(vvv->vr, ); */
	signal(SIGALRM, signal_alarm);

	//lock adv queue
	pthread_mutex_lock(&adv_q_lock);
	//obtain an VR from adv queue
	while (more && (entry = (vrrpd_vr_ds_t *)vrrp_dequeue())) {
		more = FALSE;
		
		//if der exists more entries in adv queue and current VR's timeout is 0(timer expired)
		if (entry->qnext != NULL && entry->timeout == 0){
			next_entry = entry->qnext;
			more = TRUE;
		}
		
		//obtain an instance in which current VR is found
		p = entry->parent;

	
	//if current instance is MASTER send adv and set adv timer(timeout) to adv interval and enter the VR into adv queue
		if (p->state == VRRP_STATE_MAST) {
			//vrrp_send_adv(&entry->vr, &entry->attr, entry->vfd);
	
			vrrpd_ds_t *send_inst;
			vrrpd_vr_ds_t *send_vr;

			send_vr = (vrrpd_vr_ds_t*)malloc(sizeof(vrrpd_vr_ds_t));
			send_inst = (vrrpd_ds_t*)malloc(sizeof(vrrpd_ds_t));

			memcpy(send_vr, p->vr, sizeof(vrrpd_vr_ds_t));
			memcpy(send_inst, p, sizeof(vrrpd_ds_t));
			
			send_inst->vr = send_vr;

			vrrp_send_adv(send_inst, 1);
			entry->timeout = VRRP_SEC2USEC(entry->attr.delay);
			vrrp_enqueue_impl(entry);
			if(next_entry == NULL)
				alarm(entry->attr.delay);
			else		
				alarm(next_entry->attr.delay);
			
			//vrrpd_log("\n\nADV PACKET SENT!!!YAHHHOOOOO----\n\n");
		} 
	//if current instance is not MASTER (i.e., BACKUP) state transition from BACKUP	to MASTER
		else if (p->state == VRRP_STATE_BACK){
			vrrp_state_b2m(p);
			//vrrpd_log("\n\nstate changed from BACKUP to MASTER in signal_alarm()!!!!\n");
			entry->timeout = VRRP_SEC2USEC(entry->attr.delay);
			vrrp_enqueue_impl(entry);
			if(next_entry == NULL)
				alarm(entry->attr.delay);
			else		
				alarm(next_entry->attr.delay);
		}
	}
	//unlock adv queue
	pthread_mutex_unlock(&adv_q_lock);
	//printf("\n\nOUT!!!!!!!!!!\n");
}
