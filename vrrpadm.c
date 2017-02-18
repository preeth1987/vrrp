/*
	VRRP ADMINISTRATOR: provides bash like client interface, which accepts commands from user and processes it...
*/
#include "header.h"

#include "libvrrp.h"

void process_opt_flags(char *opts, int *pree_mp, int *acpt_mp);

/*pointer to a function that returns nothing and accepts 2 arguments */
typedef int cmd_func_t(int, char **);

/*structure to hold command and the pointer to a function for that command*/
typedef struct {
	char		*name;
	cmd_func_t	*fn;
} cmd_t;

/*declare pointer to functions here*/
cmd_func_t cmd_create, cmd_remove, cmd_startup, cmd_shutdown, cmd_modify, cmd_show, cmd_exit, cmd_getx;

/*array of all command names and pointer to respective functions*/
static cmd_t cmds[] = {
	{ "create",	cmd_create },
	{ "delete",	cmd_remove },
	{ "startup",	cmd_startup },
	{ "shutdown",	cmd_shutdown },
	{ "modify",	cmd_modify },
	{ "show",	cmd_show },
	{ "getx",       cmd_getx },
	{ "exit",       cmd_exit },
	{ "quit",       cmd_exit },
	{ "q",       cmd_exit },
};

/*main function accepts commands on vrrpadm CLI*/

int main(int argc, char* argv[]){

	int	i,j;
	cmd_t	*cp;
	char	cmd[CMDMAXLEN+1], c;
	char	**subcmd;

	/*allocate memory to 2 dimensional array*/

	subcmd = malloc( 30 * sizeof(char*));

	for(i = 0; i < 30; i++)
		subcmd[i] = malloc(CMDMAXLEN * sizeof(char));

	/*repeat infinitely and accept commands*/
	for(;;){
		printf("\nvrrpadm> ");
		i=0,j=0;
		/*accept the command from std input(keyboard)*/
		while((c=fgetc(stdin))!='\n'){
			if(c==' '){
				subcmd[i][j]='\0';				
				j=0;i++;continue;		
			}
			subcmd[i][j++]=c;
		}
		subcmd[i][j]='\0';			
		argc=i+1;
		/*search the user entered command in the predefined array declared above*/
		for (i = 0; i < sizeof (cmds) / sizeof (cmd_t); i++) {
			/*copy  the ith entry into cp*/	
			cp = &cmds[i];
			/*compare command with argument*/
			if (strcmp(subcmd[0], cp->name) == 0) {
				/*if match found call appropriate function */
				if((cp->fn(argc, subcmd)) == 0)
					printf("SUCCESSFULL EXECUTION OF COMMAND\n");
				break;
			}
		}
	}

	return 0;
}
/*cmd_****() functions accepts arguments parses it and sends data to corresponding functions in libvrrp, which
  *is later sent to vrrpd*/

/*function called when create command is executed*/
int cmd_create(int argc, char *argv[]){

	print_args("CREATE INSTANCE", argc, argv);

	vrrp_inst_t inst;//for a instance
	
	vrrp_attr_t va;//attributes of VR
	
	//char *pip;//temp store primary IP for VR
	vrrp_vr_t vr[32];	/* TBD 32 should be changed */
	char *pip[32];
	//pip = (char *)malloc(32*sizeof(char));
	/* set vr array to all-zero */
	bzero((void *)vr, sizeof (vrrp_vr_t) * 32);
	bzero((void *)pip, sizeof (char *) * 32);
	
	int ppip;
	
	vrrp_vr_t *cv;//pointer to current VR

	vrrp_ret_t ret;
	int c;

	/*Default values*/
	va.pri = 100;
	va.delay = 1;	
	va.pree_mode = TRUE;
	va.accept_mode = TRUE;
	inst.vi_state = VRRP_STATE_INIT;
	
	while((c=getopt(argc,argv, "A:d:i:j:o:p:v:")) != EOF){
		printf("optarg : %s\n", optarg);
	switch(c){

		case 'A':
			if(parse_vrip(optarg, cv)<0){
				//printf("Associated IP addresses incorrect\n");
				//return;
			}
			break;
		
		case 'd':
			va.delay = strtol(optarg, NULL, 0);
			break;
		
		case 'i':
			strcpy(cv->vr_ifname, optarg);
			break;
		
		case 'j':
			//pip=optarg;
			pip[ppip] = optarg;
			parse_vrip(optarg, cv);
			break;
		
		case 'o':
			process_opt_flags(optarg, (int*)&va.pree_mode, (int*) &va.accept_mode);
			break;

		case 'p':
			va.pri = strtol(optarg, NULL, 0);
			break;
		
		case 'v':
			cv = &inst.vi_vr;
			cv->vr_id = strtol(optarg, NULL, 0);
			ppip=0;
			break;

		default: break;

	}
	}

	optind=1;
	opterr=0;

	//struct in_addr in;
	
	handle_ij(&inst.vi_vr, pip[0]);
	
	//printf("VRRPADM: PIP: %s\n",pip);
	
	inst.vi_va = va;
	ret = cmd_create_inst(&inst);
}

int handle_ij(vrrp_vr_t *vr, const char *pip){
	/*
	 * vr->vr_af already set before being called.
	 * vr->vr_ifname already set before being called.
	 * pip already set before being called
	 */

	return (inet_pton(vr->vr_af, pip, &(vr->vr_pip)));
}


/*function called when remove command is executed*/
int cmd_remove(int argc, char *argv[]){
	
	print_args("DELETE INSTANCE", argc, argv);
		
	vrid_t vid;
	char ifname[32];
	int af;
	int ret;

	parse_opt_ids(argc, argv, &vid, ifname, &af);

	ret = cmd_destroy_inst(vid, ifname, af);

	if(ret == VRRP_SUCCESS)
		return 0;
	
	printf("%s\n",vrrp_strerror(ret));
	
	return -1;
}

/*function called when modify command is executed*/
int cmd_modify(int argc, char *argv[]){

	print_args("MODIFY INSTANCE PROPERTIES", argc, argv);

	vrid_t vid;
	char ifname[32];
	int af;
	int ret;
	char c;

	int pri = VR_PROP_UNCHANGE;
	int delay = VR_PROP_UNCHANGE;
	int pree_mode = VR_PROP_UNCHANGE;	
	int acpt_mode = VR_PROP_UNCHANGE; 

	parse_opt_ids(argc, argv, &vid, ifname, &af);

	while((c=getopt(argc,(char**)argv, "d:p:A:P")) !=EOF){

		switch(c){
		
			case 'd': 	
				delay = strtol(optarg, NULL, 0);
				break;

			case 'p':
				pri = strtol(optarg, NULL, 0);
				break;

			/*case 'A':
				acpt_mode = strtol(optarg, NULL, 0);
				if(acpt_mode != VR_PROP_ACCEPTED || acpt_mode != VR_PROP_NOTACCEPTED){
					
					printf("accept mode can be VR_PROP_ACCEPTED or VR_PROP_NOTACCEPTED\n");
					return;
				}
				break;
			
			case 'P':
				pree_mode = strtol(optarg, NULL, 0);
				if(pree_mode != VR_PROP_PREEMPT || pree_mode != VR_PROP_UNPREEMPT){
					
					printf("accept mode can be VR_PROP_ACCEPTED or VR_PROP_NOTACCEPTED\n");
					return;
				}
				break;*/

			case 'o':
				process_opt_flags(optarg, (int*)&pree_mode, (int*)&acpt_mode);
				break;
			
			default:
				break;
				
		}
	}

	optind=1;
	opterr=0;

	ret = cmd_set_inst_prop(vid, ifname, af, pri, delay, pree_mode, acpt_mode);

	if(ret == VRRP_SUCCESS)
		return 0;
	
	printf("%s\n", vrrp_strerror(ret));
	
	return -1;
}

/*function called when shutdown command is executed*/
int cmd_shutdown(int argc, char *argv[]){

	print_args("SEND SHUTDOWN EVENT TO INSTANCE", argc, argv);

	vrid_t vid;
	char ifname[32];
	int af;
	int ret;

	parse_opt_ids(argc, argv, &vid, ifname, &af);

	ret = cmd_shutdown_inst(vid, ifname, af);

	if(ret == VRRP_SUCCESS)
		return 0;
	
	printf("%s\n", vrrp_strerror(ret));
	
	return -1;
}

/*function called when startup command is executed*/
int cmd_startup(int argc, char *argv[]){

	print_args("SEND STARTUP EVENT TO INSTANCE", argc, argv);

	vrid_t vid;
	char ifname[32];
	int af;
	int ret;

	parse_opt_ids(argc, argv, &vid, ifname, &af);

	ret = cmd_startup_inst(vid, ifname, af);

	if(ret == VRRP_SUCCESS)
		return 0;
	
	printf("%s\n", vrrp_strerror(ret));
	
	return -1;
}

/*function called when getx command is executed*/
int cmd_getx(int argc, char *argv[]){

	vrrp_vr_t *vrs;
	vrrp_advinfo_t *advs;
	
	vrid_t	vid = 0;
	char	ifname[32];
	int	af;

	vrrpd_ds_t *ix = NULL;
	
	int ret;

	parse_opt_ids(argc, argv, &vid, ifname, &af);
	
	
	ret = cmd_get_instx(vid, ifname, af, &ix);
	
	if (ret != VRRP_SUCCESS) {
		printf("%s\n", vrrp_strerror(ret)); 
		return 1;
	}

	display_vrrp_instx(ix);
	
	printf("__ next: vid = %d, intf = _%s_, af = %s\n", vid, ifname, (char*)af_str(af));
	

	free_vrrp(ix);

	return 0;

}

/*function called when show command is executed*/
int cmd_show(int argc, char *argv[]){

	print_args("SHOW INSTANCE ATTRIBUTES", argc, argv);

		vrid_t vid;
	char ifname[32];
	int af;
	int ret;

	//parse_opt_ids(argc, argv, &vid, ifname, &af);

	ret = cmd_show_all();

	if(ret == VRRP_SUCCESS)
		return 0;
	
	printf("%s\n", vrrp_strerror(ret));
	
	return -1;
}

/*function called when exit or quit command is executed*/
int cmd_exit(int argc, char *argv[]){

	printf("VRRPADM TERMINATING....\n");
	
	exit(0);
}

/*this function prints command line arguments arguments*/
int print_args(const char* cmd, int argc, char *argv[]){

	int i;
	printf("%s\n",cmd);
	printf("ARGUMENTS:\n");
	for(i=1;i<=argc;i++){
		printf("%s ",argv[i]);
	}
	printf("\n");
}

/*this function parses options and argumets for these options*/
int parse_opt_ids(int argc, char *argv[], vrid_t *vid, char *ifname, int *af){

	int taf;
	char c;
	
	extern char *optarg;
	extern int optind, optopt;


	if (vid != NULL)
		*vid = 0;
	if (ifname != NULL)
		ifname[0] = 0;		/* ifname[] is an buffer */
	if (af != NULL)
		//*af = AF_UNSPEC;
		*af = AF_INET;

	//printf("%s\n%s\n%s\n", argv[0], argv[1], argv[2]);
	//char **temp=(char**)argv;
	while((c=getopt(argc,argv,"v:a:i:")) != -1){

		switch(c){
			case 'v': 
				*vid = strtol(optarg, NULL, 0);
				break;	

			case 'a':
				taf = strtol(optarg, NULL, 0);
				if(taf == 4)
					*af = AF_INET;
				//else if(taf == 6)
				//	*af = AF_INET6;
				break;

			case 'i':
				strcpy(ifname, optarg);
				break;
			
			case '?':
			default:
				usage(); 
				break;
		}
	}

	optind=1;
	opterr=0;
	
	printf("vid: %d\naf: %d\nifname:%s\n", *vid, *af, ifname);
		
	return 0;
}

/*this function called when incorrect command is executed*/
int usage(){

	printf("incorrect use of commands..\n");
	return 0;
}

/*parse associated IP addresses and store in v*/
int parse_vrip(const char *addr_str, vrrp_vr_t *v){

	char str[256];
	char *p = str;
	int num = 1;		/* number of addresses */
	in_addr_t sin;
	size_t alen;		/* size of an IPv4 address */
	void *pa;
	int i;

	strcpy(str, addr_str);

	while (*p != '\0') {
		if (*p++ == ',')
			num++;
	}

	p = str, i = 0;
	while ((p = strtok(p, ",")) != NULL) {
		if (i == 0) {
			if (inet_pton(AF_INET, p, (void *)&sin) == 1) {
				v->vr_af = AF_INET;
				alen = sizeof (in_addr_t);
			}else {
				/* wrong address family: not 4*/
				return (-1);
			}
			v->vr_ip = malloc(alen * num);
		}
		pa = (void *)((char *)v->vr_ip + alen * i);
		if (inet_pton(v->vr_af, p, pa) != 1) {
			return (-1);
		}
		i++;
		p = NULL;
	}
	v->vr_ipnum = num;
	return (0);
}

/* to parse sub options for pree_mode and acpt_mode */
void process_opt_flags(char *opts, int *pree_mp, int *acpt_mp){
	static char *myopts[] = {
		"preempt", "un_preempt", "accept", "not_accept", NULL
	};
	enum { o_preempt = 0, o_un_preempt, o_accept, o_not_accept };

	char *subopts = strdup(opts);
	char *value;

	while (*subopts != '\0') {
		switch (getsubopt(&subopts, myopts, &value)) {
		case o_preempt:
			*pree_mp = VR_PROP_PREEMPT;
			break;
		case o_un_preempt:
			*pree_mp = VR_PROP_UNPREEMPT;
			break;
		case o_accept:
			*acpt_mp = VR_PROP_ACCEPTED;
			break;
		case o_not_accept:
			*acpt_mp = VR_PROP_NOTACCEPTED;
			break;
		}
	}
	free(subopts);
}
