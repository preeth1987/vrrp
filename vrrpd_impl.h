#define VRRP_SEC2USEC(t) (t*1000000)

#define vrrp_id		vr->vr.vr_id
#define vrrp_ifname	vr->vr.vr_ifname	
#define vrrp_af		vr->vr.vr_af
#define vrrp_pri	vr->attr.pri
#define vrrp_delay	vr->attr.delay
#define vrrp_pree	vr->attr.pree_mode
#define vrrp_accept	vr->attr.accept_mode

vrrp_ret_t vrrp_create_inst(vrrp_inst_t *inst, vrrpd_ds_t **vim);
vrrp_ret_t vrrp_create_vr(vrrp_attr_t *va, vrrp_vr_t *vr, vrrpd_vr_ds_t *new_vr);
int vrrp_check_conflict(vrid_t id, const char* ifname, int af);
vrrp_ret_t vrrp_destroy_inst(vrid_t id, const char* ifname, int af);
vrrp_ret_t vrrp_shutdown_inst(vrid_t id, const char* ifname, int af);
int vrrp_get_instx(vrid_t *id, char *ifname, int *af, vrrpd_ds_t **vix);
vrrp_ret_t vrrp_set_inst_prop(vrid_t vid, const char *intf, int af, int pri, int delay, int pree_mode, int acpt_mode);


vrrpd_vr_ds_t* vrrp_search_vr(vrid_t id, const char* ifname, int af);
vrrpd_ds_t* vrrp_search_inst(vrid_t id, const char* ifname, int af);
vrrpd_ds_t* vrrp_search_inst_full(vrid_t id, const char* ifname, int af);
vrrpd_ds_t* vrrp_search_inst_part(vrid_t id, const char* ifname, int af);

//vrrp_ret_t vrrp_send_adv(vrrp_vr_t *vr, vrrp_attr_t *va, vrrp_intf_fd_t *netfd);
vrrp_ret_t vrrp_send_adv(vrrpd_ds_t*, int);

int vrrp_enqueue(vrid_t id, const char* ifname, int af);
int vrrp_enqueue_impl(vrrpd_vr_ds_t* entry);
vrrpd_vr_ds_t*  vrrp_dequeue();
int vrrp_rmqueue(vrid_t id, const char* ifname, int af);
int vrrp_rmqueue_impl(vrrpd_vr_ds_t* entry);

void *timer_thread();
extern pthread_mutex_t adv_q_lock;

