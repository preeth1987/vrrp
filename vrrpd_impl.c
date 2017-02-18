#include "header.h"
#include "vrrp.h"
#include "libvrrp.h"
#include "vrrpd_impl.h"

//check whether the variable passed is set or not
#define vrrp_check(a) if(!(a)) return EINVAL

/*list of inatances @ runtime*/
vrrpd_ds_t inst_runtime_list;
/*list of VR's @ runtime*/
vrrpd_vr_ds_t vr_runtime_list;

//advertisement queue
vrrpd_vr_ds_t  *adv_q;

//to lock operations into adv_q
pthread_mutex_t adv_q_lock;



/*VRRPD FUNCTION TO CREATE A INSTANCE*/
vrrp_ret_t vrrp_create_inst(vrrp_inst_t *inst, vrrpd_ds_t **vim){

	/*check instanve parameters*/
	
	vrrp_ret_t ret;

	vrrp_check(inst!=NULL);
	vrrp_check(inst->vi_va.delay > 0);
	vrrp_check((inst->vi_vr).vr_ipnum > 0);
	
	vrrpd_ds_t *inst_list = &inst_runtime_list;
	vrrpd_vr_ds_t *vr_list = &vr_runtime_list;

	ret = vrrp_check_conflict(inst->vi_vr.vr_id, inst->vi_vr.vr_ifname, inst->vi_vr.vr_af);

	if(ret!= VRRP_SUCCESS)
		return VRRP_EVREXIST;

	vrrpd_ds_t *new_inst;
	vrrpd_vr_ds_t *new_vr;
	
	if((new_inst=malloc(sizeof(vrrpd_ds_t))) == NULL)
		return VRRP_ENOMEM;
	
	if((new_vr=malloc(sizeof(vrrpd_vr_ds_t))) == NULL)
		return VRRP_ENOMEM;

	bzero((vrrpd_ds_t*)new_inst, sizeof(vrrpd_ds_t));	
	bzero((vrrpd_vr_ds_t*)new_vr, sizeof(vrrpd_vr_ds_t));	

	new_inst->vr = new_vr;
	new_vr->parent = new_inst;
	
	new_inst->state = VRRP_STATE_INIT;
	new_inst->active = inst->vi_active;
	
	gettimeofday(&new_inst->state_trans_time, NULL);
	
	if(vrrp_create_vr(&(inst->vi_va), &(inst->vi_vr), new_vr) == VRRP_ENOMEM)
		return VRRP_ENOMEM;
	
	/*add new instance to list*/
	if(inst_list->next == NULL){//list empty

		inst_list->next = new_inst;
		new_inst->prev = inst_list;
		new_inst->next = NULL;
		
		*vim = new_inst;

		return VRRP_SUCCESS;
	}
	
	vrrpd_ds_t *curr=inst_list->next;
	
	//traverse the curr pointer to correct loc based on vrid
	while(curr->vrrp_id < new_inst->vrrp_id && curr->next!=NULL)
		curr = curr->next;
		
	/*append new instance*/

	if((curr -> vrrp_id) < (new_inst->vrrp_id)){

		curr->next = new_inst;
		new_inst->prev = curr;
		new_inst->next = NULL;
	}
	else{

		new_inst->prev = curr->prev;
		new_inst->next = curr;
		curr->prev->next = new_inst;
		curr->prev = new_inst;
	}
	
	*vim = new_inst;
	
	return VRRP_SUCCESS;
}

/*function to create VR. This is called inturn by other funcitons*/
vrrp_ret_t vrrp_create_vr(vrrp_attr_t *va, vrrp_vr_t *vr, vrrpd_vr_ds_t *new_vr){

	memcpy(&(new_vr->attr), va, sizeof(vrrp_attr_t));
	memcpy(&(new_vr->vr), vr, sizeof(vrrp_vr_t));

	uint16_t ip_length = 0;
	
	/*ip length = # of IP addresses * sizeof of each IP structure*/
	ip_length = (vr->vr_ipnum)*sizeof(struct in_addr);
	
	if((new_vr->vr.vr_ip = malloc(ip_length)) == NULL)
		return VRRP_ENOMEM;

	memcpy(new_vr->vr.vr_ip, vr->vr_ip,ip_length);
	return VRRP_SUCCESS;
}

/*check whether there exists any other instance with the given parameters. If NOT found it returns VRRP_SUCCESS*/
int vrrp_check_conflict(vrid_t id, const char* ifname, int af){

	/*search in each VR for instance*/
	if(vrrp_search_vr(id, ifname, af) == NULL)
		return VRRP_SUCCESS;
	else 
		return -1;
}

/*search VR. If found it returns pointer to that VR else it returns NULL*/
vrrpd_vr_ds_t* vrrp_search_vr(vrid_t id, const char* ifname, int af){

	vrrpd_ds_t* curr = NULL;
	vrrpd_ds_t* inst_list = NULL;
	
	vrrpd_vr_ds_t* vr = NULL;
	uint8_t	i;
	
	inst_list = &inst_runtime_list;

	/* check vr in  instance */
	curr = vrrp_search_inst(id, ifname, af);

	if (curr != NULL)
		return curr->vr;

	return NULL;
}

/*search the instance with given parameters*/
vrrpd_ds_t* vrrp_search_inst(vrid_t id, const char* ifname, int af){

	if (id != 0 && ifname != NULL && ifname[0]!= '\0' && af != 0)
		return vrrp_search_inst_full(id, ifname, af);
	else 
		return vrrp_search_inst_part(id, ifname, af);
}

/*Searches for instance in the current instance list i.e., inst_runtime_list. Returns pointer to the instance if found else NULL is sent*/
vrrpd_ds_t* vrrp_search_inst_full(vrid_t id, const char* ifname, int af){

	vrrpd_ds_t* curr = NULL;
	vrrpd_ds_t* inst_list = NULL;

	inst_list = &inst_runtime_list;
		

	curr = inst_list->next;

	while (curr != NULL)	{
		if ((curr->vrrp_id) == id && curr->vrrp_af == af && !strncmp(ifname, curr->vrrp_ifname, LIFNAMSIZ))
			return curr;
		curr = curr->next;
	}
	return NULL;
}

vrrpd_ds_t* vrrp_search_inst_part(vrid_t id, const char* ifname, int af){

	vrrpd_ds_t* curr = NULL;
	vrrpd_ds_t* inst_list = NULL;
	vrrpd_ds_t* rtn = NULL;

	/*  num of matched impliment due to incomplete agruments  */
	int num = 0;
	inst_list = &inst_runtime_list;
	
	
	inst_list = &inst_runtime_list;

	curr = inst_list->next;
	while (curr != NULL){
		if (id == 0 || curr->vrrp_id == id ){
			if (af == AF_UNSPEC || curr->vrrp_af == af){
				if ( ifname == NULL || ifname[0] == '\0' || !strncmp(ifname, curr->vrrp_ifname, LIFNAMSIZ)){
					if (++num > 1)
						return NULL;
					rtn = curr;
				}
			}
		}
		curr = curr->next;
	}
	return rtn;
}

/*function to remove an insatnce identified by the attributes (VRID, interface name, address family)*/
vrrp_ret_t vrrp_destroy_inst(vrid_t id, const char* ifname, int af){

		
	//check if address family is valid
	vrrp_check(af==AF_INET);
	
	vrrpd_ds_t *curr;//points to the instance to be deleted in the list
	
	//search for instance with given attributes
	curr = vrrp_search_inst(id, ifname, af);

	//check whether the instance to be deleted is in MASTER state
	// if it is in MASTER state send SHUTDOWN signal 
	if(curr->state == VRRP_STATE_MAST){
		
		vrrp_ret_t ret = vrrp_shutdown_inst(id, ifname, af);
		if(curr == NULL)
		return VRRP_ENOINST;

		/* two lists to be modified */
 	        pthread_mutex_lock(&adv_q_lock);
 	        vrrp_rmqueue(id, ifname, af);
 	        pthread_mutex_unlock(&adv_q_lock);
	
		
		//remove link to the instance to be removed from the list
		curr->prev->next = curr->next;
		
		if(curr->next != NULL)
			curr->next->prev = curr->prev;
	
		//free(curr->vr->vr.vr_ip);
		//free(curr->vr);
		//free(curr);
		return VRRP_SUCCESS;
	}
	
	//instances with given attributes does not exist.. return NO INSTANCE exists error
	if(curr == NULL)
		return VRRP_ENOINST;

	/* two lists to be modified */
        pthread_mutex_lock(&adv_q_lock);
        vrrp_rmqueue(id, ifname, af);
        pthread_mutex_unlock(&adv_q_lock);

	vrrp_state_execution(MAST_TO_INIT, curr);

	//remove link to the instance to be removed from the list
	curr->prev->next = curr->next;
	
	if(curr->next != NULL)
		curr->next->prev = curr->prev;

	//free(curr->vr->vr.vr_ip);
	//free(curr->vr);
	//free(curr);
	return VRRP_SUCCESS;
}

/*sends STARTUP signal to a particular instance*/
vrrp_ret_t vrrp_startup_inst(vrid_t id, const char* ifname, int af){

	vrrpd_ds_t *v;
	
	vrrp_check(af==AF_INET);
	vrrp_check(id>0);
	
	//search for instance with given attributes
	v = vrrp_search_inst(id, ifname, af);
	
	//NO instance with given attributes exist	
	if(v == NULL)
		return VRRP_ENOINST;

	else if(v->state != VRRP_STATE_INIT){
	
		vrrpd_log("\n\nv->state : %s\n\n",vrrp_stats[v->state]);

		return VRRP_EINVAL;
	}
	
	vrrpd_log("\n\nv->state : %s\n\n",vrrp_stats[v->state]);
	return vrrp_state_i2b(v);
}


/*send SHUTDOWN signal to an instance*/
vrrp_ret_t vrrp_shutdown_inst(vrid_t id, const char* ifname, int af){

	vrrpd_ds_t *v;
	
	vrrp_check(af==AF_INET);
	vrrp_check(id>0);

	v = vrrp_search_inst(id, ifname, af);

	if(v==NULL)
		return VRRP_ENOINST;

	if (v->state == VRRP_STATE_MAST)
		return vrrp_state_m2i(v);
	else if (v->state == VRRP_STATE_BACK)
		return vrrp_state_b2i(v);
	else
		return VRRP_EINVAL;
}

/*Function that returns a vrrp instance which has attributes (id, ifname, af) */
int vrrp_get_instx(vrid_t *id, char *ifname, int *af, vrrpd_ds_t **vix){

	if (*id == 0 && *af == AF_UNSPEC && (ifname == NULL || ifname[0] == '\0')) {
		/* the first object */
		*vix = inst_runtime_list.next;
	} else {
		*vix = vrrp_search_inst(*id, ifname, *af);
	}

	if (*vix == NULL)
		return (VRRP_ENOINST);

	if ((*vix)->next == NULL) {
		*id = 0;
		*af = AF_UNSPEC;
		ifname[0] = '\0';
	} else {
		vrrp_vr_t *n = &((*vix)->next->vr->vr);
		*id = n->vr_id;
		*af = n->vr_af;
		strcpy(ifname, n->vr_ifname);
	}
	return (VRRP_SUCCESS);

}

/*Function to set instance attributes viz., priority, delay, preempt flag, accept mode.. returns VRRP_SUCCESS on successfully changing instance properties*/
vrrp_ret_t vrrp_set_inst_prop(vrid_t vrid, const char *intf, int af, int pri, int delay, int pree_mode, int acpt_mode){

	vrrp_check(pree_mode == 0 || pree_mode == 1 || pree_mode == -1);
	vrrp_check( acpt_mode== 0 || acpt_mode == 1 || acpt_mode == -1);

	vrrpd_ds_t* curr;
	curr = vrrp_search_inst(vrid, intf, af);
	if (curr == NULL)
		return VRRP_ENOINST;

	if (pri != VR_PROP_UNCHANGE)
		curr->vrrp_pri = pri;
	if (delay != VR_PROP_UNCHANGE)
	{
		curr->vrrp_delay = delay;
		/* we should do something special because the 
		   advertisment interval has changed */
		pthread_mutex_lock(&adv_q_lock);
		if (vrrp_rmqueue(vrid, intf, af) == VRRP_SUCCESS)
			vrrp_enqueue(vrid, intf, af);
		pthread_mutex_unlock(&adv_q_lock);
	}	
	if (pree_mode != VR_PROP_UNCHANGE)
	{
		if (pree_mode == VR_PROP_PREEMPT)
			curr->vrrp_pree =TRUE;
		else if (pree_mode == VR_PROP_UNPREEMPT)
			curr->vrrp_pree = FALSE;
	}
	
	if (acpt_mode != VR_PROP_UNCHANGE)
	{
		if (acpt_mode == VR_PROP_ACCEPTED) {
			
			curr->vrrp_accept = TRUE;
		}
		else if (acpt_mode == VR_PROP_NOTACCEPTED) {
			
			curr->vrrp_accept = FALSE;
		}
	}
	return VRRP_SUCCESS;
}

/*Enter VR in advertisement queue*/
int vrrp_enqueue(vrid_t id, const char* ifname, int af){
	/* check id and af*/
	vrrp_check(id > 0);
	vrrp_check(af == AF_INET || af == AF_INET6 || af == 0);

	vrrpd_ds_t* 	parent 		= NULL;
	vrrpd_vr_ds_t* curr 		= NULL;

	uint8_t	i;

	/* check vr in  instance */
	curr = vrrp_search_vr(id, ifname, af);
	if (curr == NULL) 
		vrrpd_log("vrrp_search_vr() returns NULL.\n");

	if (curr != NULL){
		parent = curr->parent;
		if (parent == NULL)
			vrrpd_log("found a NULL parent\n");

		//if the curent instance is in master state Set the Adver_Timer to Advertisement_Interval
		if (parent->state == VRRP_STATE_MAST) 
			curr->timeout = VRRP_SEC2USEC(parent->vrrp_delay);
	
		/*if the current instance is in backup state Set the Master_Down_Timer(advertisement timer) to Master_Down_Interval*/
		else if (parent->state == VRRP_STATE_BACK)
			curr->timeout = 3 * VRRP_SEC2USEC(parent->vrrp_delay) + VRRP_SEC2USEC((256 - parent->vrrp_pri) / 256);
		//dbg_msg("before vrrp_enqueue_impl()\n");
		
		//enter the current VR into advertisement queue
		vrrp_enqueue_impl(curr);
		//dbg_msg("after vrrp_enqueue_impl()\n");
		return VRRP_SUCCESS;
	}
	return -1;

}

/*called by vrrp_enqueue() to enter into currect pos in adv queue*/
int vrrp_enqueue_impl(vrrpd_vr_ds_t* entry){
	vrrp_timeout_t sum, remain;
	vrrpd_vr_ds_t *curr = adv_q;

	/*The ualarm() function returns the number of microseconds remaining from the previous ualarm() call. If no timeouts are pending or if ualarm() has not previously been called, ualarm() returns 0.*/
	remain = sum = ualarm(entry->timeout, 0);

	if (adv_q == NULL) {/*queue empty. make adver_queue point to 1st VR... since der exists no VR set its timeout to 0*/
		adv_q = entry;
		entry->qprev = NULL;//no previous VR exists
		entry->timeout = 0;
	} else {/*move curr unless sum exceeds the current VR's timeout, bcoz timeout of this VR expires after timout of all other VR's in adver_queue expires*/
		while (curr != NULL && sum < entry->timeout) {
			sum += curr->timeout;
			curr = curr->qnext;
		}
		//found currect position of current VR in adver queue
		if (sum < entry->timeout) {//sum is less than the current VR's timeout
			/* the adver entry should be appended to the end*/
			curr->qnext = entry;
			entry->qprev = curr;
			entry->qnext = NULL;
			curr->timeout = entry->timeout - sum;//curr's timeout shld expire before entry's timeout
		} else {//place entry before curr
			entry->timeout = sum - entry->timeout;//entry's timeout shld expire before curr's timeout

			//place entry before curr
			entry->qnext = curr;
			curr->qprev = entry;
			entry->qprev = curr->qprev;
			//if curr is only one in adver_queue point adver_queue to entry else make pointer manipulation
			if (curr == adv_q) {
				adv_q = entry;
			} else {
				entry->qprev->qnext = entry;
				entry->qprev->timeout = entry->qprev->timeout - entry->timeout;
				//entry's prev VR's timeout shld expire before entry's timeout
			}
		}
		ualarm(adv_q->timeout, 0);//make next alarm @ 1st VR in adver_queue's timeout
	}

	return VRRP_SUCCESS;
}

vrrpd_vr_ds_t*  vrrp_dequeue()
{
        vrrpd_vr_ds_t *curr;

        curr = adv_q;
        if (curr != NULL)
                vrrp_rmqueue_impl(curr);

        return curr;
}

/*function to remove VR from advertisement queue*/
int vrrp_rmqueue(vrid_t id, const char* ifname, int af){
	/* check id & af*/
	vrrp_check(id > 0);
	vrrp_check(af == AF_INET || af == AF_INET6 || af == 0);
	
	vrrpd_vr_ds_t* curr = NULL;
	curr = vrrp_search_vr(id, ifname, af);//search for VR
	
	if(curr == NULL)// no VR found 
	       return -1;	

	if (curr->qnext == NULL && curr->qprev == NULL)
		/* the VR is not at the adv queue */
		return -1;
	return vrrp_rmqueue_impl(curr);// VR is in the adv queue
}

/*this function called by vrrp_rmqueue() to remove the VR from adv queue and manipulate timeout appropriately*/
int vrrp_rmqueue_impl(vrrpd_vr_ds_t* entry){
	/* is entry first node ? */
	if (entry->qprev == NULL)
		adv_q = entry->qnext;
	else
		entry->qprev->qnext = entry->qnext;
	if (entry->qnext != NULL){
		entry->qnext->qprev = entry->qprev;
		entry->qprev->timeout += entry->timeout;
		//timeout of VR prev to entry in adv queue shld expire after the timeout of entry VR
	}
	//isolate entry from adv queue
	entry->qnext = NULL;
	entry->qprev = NULL;

	return VRRP_SUCCESS;
}

int vrrp_send_adv_impl(){

        vrrpd_ds_t *v = NULL;

        v = &inst_runtime_list;
        while ((v = v->next) != NULL)
                vrrp_send_adv_impl_more(v);

        return (0);
}

int vrrp_send_adv_impl_more(vrrpd_ds_t *v){

        assert(v != NULL);

        vrrpd_vr_ds_t *r = v->vr;
        int i = 0;
        while (r != NULL) {
                //vrrp_send_adv(&r->vr, &r->attr, r->vfd);
		vrrp_send_adv(v, 1);
                r = r->next;
                i++;
        }
        return (0);
}
