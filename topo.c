#include "stdio.h"
#include "string.h"
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <unistd.h>

#define MAX 5 //maximum number of servers
#define INF 99 //value of infinity
#define SIZE_1 100 //buffer sizes
#define ARGSIZE 400 //argument sizes
#define SIZE_BUFF 2000 //buffer sizes
#define OPENDNS "8.8.8.8" //openDNS address by google
#define MAXCOUNT 3 //maximum time units to wait for the neighbour to respond

//global variables, structures
int my_id=0;
int max_servers=0;
int routing_interval=0;
struct sockaddr_in ser,client_sockaddr;
socklen_t l1;
fd_set fdset, fdset_copy;
int fdmax=0;
static int rt[MAX][MAX];//routing table which keeps all the latest cost to all the nodes in the system
static int how[MAX];//works along with the routing table to help locate the VIA route
static int my_ngh_cost[MAX];//all the neighbours with cost
static int valid_ngh[MAX];//only valid neighbours
static int ngh_max_count=0;
static int d[MAX][2];//temporary data structure to hold records
static int my_dv_changed=1;
static int covered_nodes[MAX][2];
static int loop_time=0;
static int g_packets=0;
static int crash_bit=0;
static int first_stint=1;
static int cost_change=0;
static int tax_time=0;
//structure to hold the nodes data, IP, PORT, ID
struct ser_info
{
	int ser_id;
	char ser_ip[SIZE_1];
	char ser_port[SIZE_1];
}s_info[MAX];
//part of the message format packet
struct parts
{
	in_addr_t ser_ip;
	unsigned short ser_port;
	unsigned short null;
	unsigned short ser_id;
	unsigned short cost;
};
//structure to form the header of the packet's message format
struct msg_pck
{
	unsigned short noofupdates;
	unsigned short ser_port;
	in_addr_t ser_ip;
	struct parts p[MAX];//has data parts, as defined above
};
//ends

//function declaration
void fire_client(struct msg_pck requeststring, char *to_name,char *to_port);
void extractFilename(char *t,char *desti);
int min(int range);
char *get_myip();
void display_routing();

void parse_struct(struct msg_pck pk);
struct msg_pck prepare_packet(int noofupdates);
void send_rt();
int do_algo();
void chk_sender_status();
void reset_counters();
void reset_d();

void init_fill_cost();
void set_init_ngh_cost(int j,int cost);
void change_cost(int target_node,int new_cost,int action);
void send_cost_to(int id,int new_cost,int action);

void kill_ngh(int nodeid);
void kill_all_ngh();
int isvalid_ngh(int nodeid);
void init_ngh();

void topo_set(char *filename);
void errormsg(char *msg);
void successmsg(char *msg);
void args_parse(char *strPara);
void parse_cmd(char *strPara);

void start_server();
//ends

//functions definitions

/*sends the packet to server*/
void fire_client(struct msg_pck requeststring, char *to_name,char *to_port)
{
	int s,len,result;
	char buff[SIZE_BUFF];
	struct sockaddr_in c;

	s=socket(AF_INET,SOCK_DGRAM,0);
	c.sin_family=AF_INET;
	c.sin_addr.s_addr=inet_addr(to_name);
	c.sin_port=atoi(to_port);
	len=sizeof(c);
	if(sendto(s, &requeststring, sizeof(requeststring), 0,(struct sockaddr*)&c,len)==-1)
		puts("\nsending_error\n");
}
/*it returns the ip of the system running the program using openDNS*/
char *get_myip()
{
	int sd;
	struct sockaddr_in sa;
	int sa_len;
	sa.sin_family = AF_INET;
	sa.sin_addr.s_addr = inet_addr(OPENDNS); //public google DNS
	sa.sin_port = 0;
	sd = socket(AF_INET,SOCK_DGRAM,0);
	sa_len=sizeof(sa);
	connect(sd,(struct sockaddr *)&sa,sa_len);
	sa_len = sizeof(sa);
	getsockname(sd,(struct sockaddr*)&sa,&sa_len);
	return inet_ntoa(sa.sin_addr);
}
void all() /*function not being used*/
{
	int i=0;
	for(i=0;i<max_servers;i++)
		printf("\nip:%s port:%s my_id%d",s_info[i].ser_ip,s_info[i].ser_port,my_id);
	display_routing();
	for(i=0;i<ngh_max_count;i++)
		printf("\nngh:%d ",valid_ngh[i]);
}
/*display routing table*/
void display_routing()
{
	char str[SIZE_BUFF];
	int i;
	sprintf(str,"\nMY ID:%d\nROUTING TABLE with via nodes\n",my_id+1);
	puts(str);
	for(i=0;i<max_servers;i++)
	{
		if(rt[my_id][i]==INF)
			printf("TO:%d VIA:%d COST:INFINITY\n",i+1,how[i]+1);
		else
			printf("TO:%d VIA:%d COST:%d\n",i+1,how[i]+1,rt[my_id][i]);
	}
}
/*argument parser*/
void extractFilename(char *t,char *desti)
{
	int i =0;
	t=t+3; //cross -x and a space
	for(i=0;t[i]!=' ';i++) 
		desti[i]=t[i];
	desti[i]='\0';
}
void errormsg(char *msg)
{
	char str[SIZE_1];
	int i=0;
	for(i=0;i<strlen(msg);i++)
		if(msg[i]=='\n')
		      msg[i]=' ';
	sprintf(str,"40 %s COMMAND ERROR",msg);
	puts(str);
}
void successmsg(char *msg)
{
	char str[SIZE_1];
	int i=0;
	for(i=0;i<strlen(msg);i++)
		if(msg[i]=='\n')
		      msg[i]=' ';
	sprintf(str,"40 %s SUCCESS",msg);
	puts(str);
}
/*takes care of setting a new cost value and checks if neighbour is to be deleted or not*/
void change_cost(int target_node,int new_cost,int action)
{
	my_ngh_cost[target_node]=new_cost;
	cost_change=1;
	if(action==7 || action==8)//disable-->means kill OR if chk_sender_status wants to kill somebody
		kill_ngh(target_node+1);
}
/*checks to see if all the neighbours have responded and takes action if not*/
void chk_sender_status()
{
	int i=0;
	for(i=0;i<max_servers;i++)
	{
		if(isvalid_ngh(i+1))
		{
			if(covered_nodes[i][0]==0)
			{
				if(abs(covered_nodes[i][1]-tax_time)>=MAXCOUNT)
					  change_cost(i,INF,8);
			}
		}
	}
	if(tax_time==65535)
		  tax_time=-1;
	tax_time++;
}
/*parser for incoming packet, also checks whether all neighbours have responded or not*/
void parse_struct(struct msg_pck pk)
{
	char strmsg[SIZE_1];
	if(pk.noofupdates<MAX)
	{
		int i,sender_id;
		//increment packets
		g_packets++;
		for(i=0;i<max_servers;i++)
		{
			if(pk.p[i].cost==0)
				sender_id=pk.p[i].ser_id;
		}
		sprintf(strmsg,"RECEIVED A MESSAGE FROM SERVERID: %d",sender_id);
		successmsg(strmsg);
		if(pk.noofupdates!=0)
		{
			//extract the struct
			int i,tmp_id, k=pk.noofupdates;
			for(i=0;i<max_servers;i++)
			{
				tmp_id = pk.p[i].ser_id -1;
				strcpy(s_info[tmp_id].ser_ip,inet_ntoa(*(struct in_addr*)&pk.p[i].ser_ip));
				sprintf(s_info[tmp_id].ser_port,"%d",pk.p[i].ser_port);
				rt[sender_id-1][tmp_id]=pk.p[i].cost;
			}
		}
		//sender's job is done
		covered_nodes[sender_id-1][0]++;
		covered_nodes[sender_id-1][1]=tax_time;
	}
	else
	{
		int temp_indx=pk.p[1].ser_id;
		sprintf(strmsg,"\nRECEIVED A MESSAGE FROM SERVERID: %d",pk.p[1].ser_id);
		successmsg(strmsg);
		change_cost(temp_indx-1,pk.p[0].cost,pk.noofupdates);//6 for update and 7 for disable
	}
}
/*message preparation for update or disable calls*/
void send_cost_to(int sendto_id,int new_cost,int action) //action =6 for update and 7 for disable
{
	struct msg_pck pk;

	pk.noofupdates=action;
	pk.ser_port = atoi(s_info[my_id].ser_port);
	pk.ser_ip = inet_addr(s_info[my_id].ser_ip);

	pk.p[0].ser_ip = inet_addr(s_info[sendto_id].ser_ip);
	pk.p[0].ser_port = atoi(s_info[sendto_id].ser_port);
	pk.p[0].null=0;
	pk.p[0].ser_id = sendto_id;

	if(action==6) //send update packet only
		pk.p[0].cost = new_cost;
	else if(action==7) //send disable packet
		pk.p[0].cost=INF;
	//my data again
	pk.p[1].ser_ip = inet_addr(s_info[my_id].ser_ip);
	pk.p[1].ser_port = atoi(s_info[my_id].ser_port);
	pk.p[1].null=0;
	pk.p[1].ser_id = my_id+1;

	fire_client(pk,s_info[sendto_id].ser_ip,s_info[sendto_id].ser_port);
}
/*standard message preparation*/
struct msg_pck prepare_packet(int noofupdates)
{
	struct msg_pck mp;
	int i=0;
	mp.noofupdates = noofupdates;
	mp.ser_port = atoi(s_info[my_id].ser_port);
	mp.ser_ip = inet_addr(s_info[my_id].ser_ip);
	for(i=0;i<max_servers;i++)
	{
		mp.p[i].ser_ip = inet_addr(s_info[i].ser_ip);
		mp.p[i].ser_port = atoi(s_info[i].ser_port);
		mp.p[i].null=0;
		mp.p[i].ser_id = s_info[i].ser_id;
		mp.p[i].cost = rt[my_id][s_info[i].ser_id-1];
	}
	return mp;
}
/*runs the algorithm, prepares the packet accordingly and sends it*/
void send_rt()
{
	struct msg_pck pk;
	int change,i;
	change=do_algo();
	pk=prepare_packet(change);
	for(i=0;i<max_servers;i++)
	{
		if(isvalid_ngh(i+1))
			fire_client(pk,s_info[i].ser_ip,s_info[i].ser_port);
	}
}
/*reset temp array d*/
void reset_d()
{
  int i=0;
  for(i=0;i<MAX;i++)
  {
    d[i][0]=0;
    d[i][1]=0;
  }
}
/*get the minimun of d[][] vector*/
int min(int range)
{
	int i=0,index=0,m=d[0][0];
	for(i=0;i<range;i++)
	{
		if(d[i][0]<m)
		{
			m=d[i][0];
			index=i;
		}
	}
	return index;
}
/*Bellman ford algorithm for the current node*/
int do_algo()
{
	int i,j,r,k=0,value=0,via=0;
	my_dv_changed=0;
	i=my_id;
	for(j=0;j<max_servers;j++)
	{
		if(i!=j)
		{
			for(r=0;r<max_servers;r++)
			{
				if(r==i) continue;
				d[k][0]=my_ngh_cost[r]+rt[r][j];
				d[k][1] = r;
				k++;
			}
			value=d[min(k)][0];
			via = d[min(k)][1];
			if(rt[i][j]!=value)
			{
				my_dv_changed=1;
				rt[i][j]=value;
				how[j]=via;
			}
			if(cost_change==1)
			{
				my_dv_changed=1;
				rt[i][j]=value;
				how[j]=via;
				cost_change=0;
			}
			if(first_stint==1)
			{
				first_stint=0;
				my_dv_changed=1;
			}
			reset_d();
			k=0;
		}
	}
	return my_dv_changed;
}
/*Initializes my_ngh_cost[] vector sets it to INF*/
void init_fill_cost()
{
	int i;
	for(i=0;i<MAX;i++)
		my_ngh_cost[i]=INF;
}
/*sets the costs of all the neighbours*/
void set_init_ngh_cost(int j,int cost)
{
	my_ngh_cost[j]=cost;
}
/*Initializes routing table after setting up the neighbour's costs and sets the "how" array*/
void init_routing()
{
	int i,j;
	for(i=0;i<max_servers;i++)
	{
		rt[my_id][i]=my_ngh_cost[i];
		for(j=0;j<max_servers;j++)
		{
			if(my_id!=i)
				rt[i][j]=INF;
		}
	}
	how[my_id]=my_id;
}
/*deletes ALL the neighbours from the ngh vector, sets it to -1 and decreases the ngh_max_count variable, used to satifsy CRASH condition*/
void kill_all_ngh()
{
	int i=0;
	for(i=0;i<ngh_max_count;i++)
	{
		if(valid_ngh[i]!=-1)
		{
			change_cost(valid_ngh[i]-1,INF,7);
			valid_ngh[i]=-1;
		}
	}
}
/*deletes the neighbour from the ngh vector, sets it to -1 and decreases the ngh_max_count variable*/
void kill_ngh(int nodeid)
{
	char str[SIZE_1];
	int i=0;
	for(i=0;i<ngh_max_count;i++)
	{
		if(valid_ngh[i]==nodeid)
		{
			valid_ngh[i]=-1;
			sprintf(str,"LOST NEIGHBOUR serverID:%d\n",nodeid);
			puts(str);
			return;
		}
	}
}
/*checks whether the node is my neighbour or not*/
int isvalid_ngh(int nodeid)
{
	int i=0;
	for(i=0;i<ngh_max_count;i++)
	{
		if(valid_ngh[i]==nodeid)
			return 1;
	}
	return 0;
}
/*sets the valid_ngh[] vector with valid neighbours, whose cost is neither 0 or INF*/
void init_ngh()
{
	int i,k=0;
	for(i=0;i<max_servers;i++)
	{
		if(my_ngh_cost[i]!=0 && my_ngh_cost[i]!=INF)
			valid_ngh[k++]=i+1;
	}
	ngh_max_count=k;
}
/*Initializes the topology of the system by reading from the topology file given at startup*/
void topo_set(char *filename)
{
	FILE *fp;
	char buff[SIZE_1];
	int i,j;
	int n_sers,ngh;
	int temp_id,temp_ngh_id,temp_cost;

	fp=fopen(filename,"r");
	if(fp==NULL)
	{
	  errormsg("FILE NOT FOUND");
	  exit(1);
	}
	fscanf(fp,"%d",&n_sers);
	//setting max_servers
	max_servers = n_sers;

	fscanf(fp,"%d",&ngh);
	for(i=0;i<n_sers;i++)
		fscanf(fp,"%d %s %s",&s_info[i].ser_id,&s_info[i].ser_ip,&s_info[i].ser_port);
	for(i=0;i<ngh;i++)
	{
		fscanf(fp,"%d %d %d",&temp_id,&temp_ngh_id,&temp_cost);
		set_init_ngh_cost(temp_ngh_id-1,temp_cost);
	}
	//setting my_id and cost to myself=0
	my_id = temp_id-1;
	strcpy(s_info[my_id].ser_ip,get_myip());
	set_init_ngh_cost(my_id,0);
	//initialize routing table
	init_routing();
	//prepare neighbours
	init_ngh();
	fclose(fp);
}
void parse_cmd(char *strPara)
{
	char *tmp=NULL;
	int i=0;
	if(strlen(strPara)==0)
		return;
	
	for(i=0;i<strlen(strPara);i++)
		strPara[i]=tolower(strPara[i]);
	
	tmp=strstr(strPara,"update");
	if(tmp!=NULL)
	{
		int me,target_node,c;
		char cost[2],crap[10];
		sscanf(strPara,"%s %d %d %s",&crap,&me,&target_node,&cost);
		if(me-1!=my_id)
		{
			errormsg(strPara);
			return;
		}
		if(strstr(cost,"inf")!=NULL)
			c=INF;
		else
			c=atoi(cost);

		change_cost(target_node-1,c,6);//change my cost array with appropriate action
		send_cost_to(target_node-1,c,6);//action is for update

		successmsg(strPara);
		tmp=NULL;
		return;
	}
	tmp=strstr(strPara,"step");
	if(tmp!=NULL)
	{
		send_rt();
		successmsg(strPara);
		return;
	}
	tmp=strstr(strPara,"packets");
	if(tmp!=NULL)
	{
		char str[SIZE_1];
		sprintf(str,"packets since the last time: %d",g_packets);
		g_packets=0;
		puts(str);
		successmsg(strPara);
		return;
	}
	tmp=strstr(strPara,"disable");
	if(tmp!=NULL)
	{
		int target_node;
		char crap[20];
		sscanf(strPara,"%s %d",&crap,&target_node);
		if(isvalid_ngh(target_node)==1)
		{
			change_cost(target_node-1,INF,7);//with appropriate action
			send_cost_to(target_node-1,INF,7);
			successmsg(strPara);
			return;
		}
		else
			errormsg(strPara);
		return;
	}
	tmp=strstr(strPara,"crash");
	if(tmp!=NULL)
	{
		crash_bit=1;
		kill_all_ngh();
		
		FD_ZERO(&fdset);
		FD_ZERO(&fdset_copy);
		FD_SET(fileno(stdin),&fdset);
		fdset_copy = fdset;
		fdmax=1;
		
		successmsg(strPara);
		return;
	}
	tmp=strstr(strPara,"display");
	if(tmp!=NULL)
	{
		display_routing();
		successmsg(strPara);
		return;
	}
	else
		errormsg(strPara);
}
void args_parse(char *strPara)
{
	char *tmp=NULL;
	tmp = strstr(strPara, "-t");
	if(tmp!=NULL)
	{
		char buff[SIZE_1];
		extractFilename(tmp,buff);
		init_fill_cost();
		topo_set(buff);
		tmp=NULL;
	}
	tmp=strstr(strPara,"-i");
	if(tmp!=NULL)
	{
		char buff[SIZE_1];
		extractFilename(tmp,buff);
		routing_interval=atoi(buff);
		if(routing_interval>0)
			return;
		else
		{
			errormsg("-i error");
			exit(1);
		}
		tmp=NULL;
	}
	else
		errormsg(strPara);
}
/*resets all the counters for the SELECT command*/
void reset_counters()
{
	int i=0;
	for(i=0;i<MAX;i++)
	{
		covered_nodes[i][0]=0;
		covered_nodes[i][1]=tax_time;
	}
	loop_time=-1;
}
/*starts the server*/
void start_server()
{
	char buff[SIZE_BUFF];
	int len,s_return;
	int ser_fd;
	struct timeval tv;
	tv.tv_sec = routing_interval;
	tv.tv_usec=0;

	ser_fd = socket(AF_INET,SOCK_DGRAM,0);

	ser.sin_family=AF_INET;
	ser.sin_addr.s_addr=inet_addr(s_info[my_id].ser_ip);
	ser.sin_port = atoi(s_info[my_id].ser_port);
	len = sizeof(ser);

	bind(ser_fd,(struct sockaddr *)&ser,len);

	FD_ZERO(&fdset);
	FD_ZERO(&fdset_copy);

	FD_SET(fileno(stdin),&fdset);
	FD_SET(ser_fd,&fdset);

	fdset_copy = fdset;

	fdmax=ser_fd+1;

	while(1)
	{
		s_return=0;
		s_return=select(fdmax,&fdset,NULL,NULL,&tv);
		if(s_return==0)
		{
			//calculate the routes and send the DV to neighbours, if updated
			send_rt();
			tv.tv_sec=routing_interval;
		}
		if(FD_ISSET(ser_fd,&fdset))
		{
			struct msg_pck pk;
			l1=sizeof(client_sockaddr);
			memset(&pk,0,sizeof(pk));
			if(recvfrom(ser_fd, (char *)&pk, sizeof(pk), 0,(struct sockaddr *)&client_sockaddr,&l1)!=-1)
				parse_struct(pk);
			else
				puts("PACKET Error");	
		}
		if(FD_ISSET(fileno(stdin),&fdset))
		{
			tax_time--;
			fgets(buff,sizeof(buff),stdin);
			parse_cmd(buff);
		}
		fdset=fdset_copy;
		//check if all the neighbours are alive or not
		chk_sender_status();
		if(loop_time==MAXCOUNT)
			reset_counters();
		loop_time++;
	}
	close(ser_fd);
}
int main(int argc, char *argv[])
{
	char args[ARGSIZE];
	int i=0;
	for(i=0;i<argc;i++)
	{
		strcat(args,argv[i]);
		strcat(args," ");
	}
	args_parse(args);
	start_server();
}
