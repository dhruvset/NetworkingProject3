README FILE
===========

SOURCE CODE FILENAME: topo.c

1) value of INFINITY is set to 99. Is defined in the code as "#define INF 99" @line 13
2) maximum number of servers are set to 5. Defined as "#define MAX 5" @line 12
3) maximum number of time units for neighbors to respond is set to 3. Defined as "#define MAXCOUNT 3" @line 18
4) Routing table is a 2D integer array. Define as rt[MAX][MAX] (@line 28) along with another integer 1D array how[MAX] (@line 29) is used to store next-hop nodes.
5) The cost of all the neighbors are stored in the array my_ngh_cost[MAX] (@line 30)
6) The "packets" command displays the last number of Distance Vector (UPDATE) packets received since the last time the command was issued.
7) The program does not counter "count to infinity" and thus requires a number of iterations for the routing tables to stabalized.

SERVER INFORMATION
==============
The server information is stored in the structure "struct ser_info". It stores the ID, IP and Port of the servers.

MESSAGE FORMAT
===========
The message format is based on a structure "struct msg_pck"..
Details of structure "msg_pck": (@line 58 )
1) noofupdates 	(16bits - unsigned short)
2) ser_port		(16bits - unsigned short)
3) ser_ip		(32bits - in_addr_t)
4) and a variable of structure "struct parts", whose details are given below.

Details of structure "parts": (@line 49)
1) ser_ip 		(32bits - in_addr_t)
2) ser_port 		(16bits - unsigned short)
3) null 		(16bits - unsigned short)
4) ser_id 		(16bits - unsigned short)
5) cost 		(16bits - unsigned short)

The message format begins with the object of "msg_pck" which servers as the header of the packet and a variable of "parts" which is treated as the data part.

element "noofupdates" is being used to distinguish between:
1) update (value: 6) -> having only 1 data row. (@line 269)
2) disable (value:  7) -> have only 1 data row. (@line 271)
3) regular packet (value: 5) -> having only 5 data rows. (@line 286)

Bellman Ford algorithm implementation (@line 337)
================================
The distance vector of each of the servers is saved in the routing table (rt[MAX][MAX]) and the minimum distance is calculated based 
on the cost (of the neighbour's links) and the distance vectors. If any field in the routing table changes the distance vector of current node
is sent to the neighbours.

IMPLEMENTATION DETAILS
================
1) Read the topology file and fill all the data structures: my_ngh_cost[MAX], rt[MAX][MAX] and valid_ngh[MAX]
2) Start the server and listen for two FDs STDIN and SERVER_FD.
3) The select()'s return value is used to check whether it has returned due to timeout or their is an activity on STDIN/SERVER_FD.
4) On the other hand, the function send_rt() (@line 300) is used to run the Bellman ford algorithm and check to see if there are any changes in the routing table.
     If Yes, the distance vector os the node is sent to all its neighbours. The above message format is followed throughout the system. Also, the elements of the packet
      are not being converted using htonl() or retrieved back by ntohl() because using them the elements of the structure were returning null/zero values.
5) On receiving the distance vectors, all the vectors are stored in the routing table and the algorithm runs each time a new vector comes in.
     Also, a track is kept (using a variable tax_time present in chk_sender_status() @line197) from which neighbor node the packet has come.
     This helps in checking whether the node is still alive or not. If the node does not responds for more than 3 times the neighbour node is declared dead.
6) User commands are being processed as and when they are issued.
7) For UPDATE command, the cost is updated locally and also is sent to the neighbor connected to that link. For DISABLE command, 
     the cost is updated to INFINITY and the neighbor is removed from the neighbor array. Hence, disabling the link. For CRASH command, 
     all the neighbors are removed and the cost of all the links are set to INFINITY.
8) The FD_SET for CRASH command changes to only STDIN. Hence, no packets will now be received. But, the program will still respond to commands from STDIN.
9) If the cost of link is set to INFINITY or cost is increased, the value of the destination would increase slowly (shows count to infinity).

SAMPLE TOPOLOGY FILE (I used for one of the nodes):
===================
3
3
1 128.1.1.1 9002
2 128.1.1.2 3003
3 128.1.1.3 8009
1 2 4
1 3 9 

ROUTING TABLE IS DISPLAYED AS FOLLOWS:
===========================
MY ID:1
ROUTING TABLE with via nodes

TO:1 VIA:1 COST:0
TO:2 VIA:2 COST:50
TO:3 VIA:2 COST:51

In the above output format, the MY ID:1 is the id of the current node. VIA is the next hop-node ID and TO: is the destination node ID.