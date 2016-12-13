#include "runner_pt.h"

runner_pt::runner_pt(int argc, char *argv[])
{
	MPI_Init(&argc,&argv);
	MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);
	MPI_Comm_size(MPI_COMM_WORLD, &world_size);
	sys = NULL;
	jobfile = argv[1];
	parser parsedfile(jobfile);
	if (argc>2) walltime=atof(argv[2]);
	else walltime = parsedfile.return_value_of<double>("walltime");
	if (argc>3) chktime=atof(argv[3]);
	else chktime  = parsedfile.return_value_of<double>("checkpointtime");
	if (argc>4) {//default is seconds
		if (argv[4][0]=='h') {
			walltime*=3600;
			chktime*=3600;
		}
		if (argv[4][0]=='m') {
			walltime*=60;
			chktime*=60;
		}
	}
	taskfiles= parsedfile.return_vector< string >("@taskfiles");		
	time_start = MPI_Wtime();
	time_last_chkpt = time_start;
	statusfile = parsedfile.value_or_default<string>("statusfile",jobfile+".status");
	masterfile = parsedfile.value_or_default<string>("masterfile",jobfile+".master");
	acceptfile = parsedfile.value_or_default<string>("acceptfile",jobfile+".accept");
	positionsfile = parsedfile.value_or_default<string>("positionfile",jobfile+".pos");
	STATUS = new std::ofstream (statusfile.c_str(), std::ios::out|std::ios::app);
	if(my_rank == MASTER) {
		pt_rng = new randomnumbergenerator();
		pt_node_waits_for_global_update= new bool[world_size-1];
		for (int i=0;i<world_size-1;++i) 
			pt_node_waits_for_global_update[i]=false;
		pt_N_global_update_waiter = 0;
		N_exit = 0;
	}
}

runner_pt :: ~runner_pt()
{
	(*STATUS).close();
	delete STATUS;
	delete sys;
	if(my_rank == MASTER) delete pt_rng;
}

void runner_pt :: start()
{
	if(my_rank == MASTER) {
		M_read();
		M_wait();
	}
	else what_is_next(S_IDLE);
}

bool runner_pt :: is_chkpt_time()
{
	if((MPI_Wtime() - time_last_chkpt)> chktime){
		time_last_chkpt = MPI_Wtime();
		return true;
	} 
	else return false;
}

bool runner_pt :: time_is_up()
{
	return ((MPI_Wtime() - time_start)>walltime);
}

void runner_pt :: M_update(int node)
{
	int node_status;
	MPI_Status stat;
	MPI_Recv(&node_status, 1, MPI_INT, node, T_STATUS, MPI_COMM_WORLD, &stat);
	//(*STATUS) << my_rank << ": Status " << node_status << " from " << node << "\n";
	if(node_status == S_GLOBAL_UPDATE) {
		pt_node_waits_for_global_update[node-1]=true;
		pt_N_global_update_waiter++;
		if (pt_N_global_update_waiter == world_size-1) {
			for (int i=1;i<world_size;++i) {
				M_send_action(A_GLOBAL_UPDATE, i);
				pt_node_waits_for_global_update[i-1]=false;
			}
			pt_N_global_update_waiter=0;
			M_global_update();
		}
	}
	if (node_status == S_CHECKPOINT) {
		one_task act;
		int task_comm_size = sizeof(one_task) / sizeof(char);
		MPI_Recv((char*) &act, task_comm_size, MPI_CHAR, node, T_STATUS, MPI_COMM_WORLD, &stat);
		pt_node_steps_done[node-1]=act.steps_done;
		pt_node_mes_done[node-1]=act.mes_done;
#ifdef MCL_POS
		if (node == 1)
			M_report_pt_positions();
#endif
	}	
	if (node_status == S_IDLE) {
		if(time_is_up()) M_send_action(A_EXIT,node);
		else {
			if (pt_node_order[node-1]==-1) {
				++pt_node_task[node-1];
				if (pt_node_task[node-1]==(int)tasks.size()) 
					pt_node_task[node-1]=-1;
				if(pt_node_task[node-1]<0) M_send_action(A_EXIT,node);
				else {
					M_send_action(A_NEW_JOB,node);
					pt_node_order[node-1]     =node-1;
					pt_node_mes_done[node-1]  =0;
					pt_node_steps_done[node-1]=0;
					++tasks[pt_node_task[node-1]].run_counter;
					tasks[pt_node_task[node-1]].mes_done  =pt_node_mes_done[node-1];
					tasks[pt_node_task[node-1]].steps_done=pt_node_steps_done[node-1];
					tasks[pt_node_task[node-1]].pt_pos    =pt_node_order[node-1];
					char * ptr = (char*) &tasks[pt_node_task[node-1]];
					int task_comm_size = sizeof(one_task) / sizeof(char);
					MPI_Send(ptr, task_comm_size, MPI_CHAR, node, T_NEW_JOB, MPI_COMM_WORLD);
				}
			}
			else {
				M_send_action(A_CONTINUE_JOB,node);
				tasks[pt_node_task[node-1]].mes_done  =pt_node_mes_done[node-1];
				tasks[pt_node_task[node-1]].steps_done=pt_node_steps_done[node-1];
				tasks[pt_node_task[node-1]].pt_pos    =pt_node_order[node-1];
				char * ptr = (char*) &tasks[pt_node_task[node-1]];
				int task_comm_size = sizeof(one_task) / sizeof(char);
				MPI_Send(ptr, task_comm_size, MPI_CHAR, node, T_NEW_JOB, MPI_COMM_WORLD);

			}
		}
	}
	if (node_status == S_BUSY) {
		M_send_action( ((time_is_up()) ? A_EXIT : A_CONTINUE), node);
	}
	if (node_status == S_FINISHED) {
		one_task act;
		int task_comm_size = sizeof(one_task) / sizeof(char);
		MPI_Recv((char*) &act, task_comm_size, MPI_CHAR, node, T_STATUS, MPI_COMM_WORLD, &stat);
		pt_node_order[node-1]=-1;
		tasks[act.task_id].run_counter--;
		if (tasks[act.task_id].run_counter==0) {
			tasks[act.task_id].is_done = 1;
			M_send_action(A_PROCESS_DATA_NEW_JOB,node);
			M_report_acc_ratio();
			pt_moves = 0;
			for (uint i=0;i<pt_accepted.size();++i) 
				pt_accepted[i]=0;
		}
		else M_send_action(A_NEW_JOB, node);
	}	
	if(node_status != S_GLOBAL_UPDATE && node_status != S_BUSY) {
		M_write();
		M_report();
	}
}

void runner_pt :: M_send_action(int action, int to)
{
	//(*STATUS) << my_rank << ": Action "<<action<<" to " << to << "\n";
	MPI_Send(&action, 1, MPI_INT, to, T_ACTION, MPI_COMM_WORLD);
	if(action == A_EXIT) N_exit ++;
}

void runner_pt :: M_wait()
{
	//(*STATUS) << my_rank << ": Waiting..." <<"\n";
	while (N_exit + pt_N_global_update_waiter != world_size-1) {
		MPI_Status stat;
		int flag = 0;
		while(!flag)
			MPI_Iprobe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &flag, &stat);
		if(stat.MPI_TAG == T_STATUS)
			M_update(stat.MPI_SOURCE);
		else {
			cerr<<my_rank<<": WARNING "<< stat.MPI_TAG<<" from "<<stat.MPI_SOURCE<<"\n";
		}
	}
	M_end_of_run();
}

void runner_pt :: M_write()
{
	odump d(masterfile);
	d.write(pt_moves);
	d.write< one_task >(tasks);
	d.write< int >(pt_node_task);
	d.write< int >(pt_node_order);
	d.write< int >(pt_node_mes_done);
	d.write< int >(pt_node_steps_done);
	d.write< int >(pt_accepted);
#ifdef MCL_HISTOGRAM
	d.write< int >(histogram_up);
	d.write< int >(histogram_down);
#endif
	d.close();
}

void runner_pt :: M_read()
{
	idump d(masterfile);
	if (d.good()) {
		d.read(pt_moves);
		d.read< one_task >(tasks);
		d.read< int >(pt_node_task);
		d.read< int >(pt_node_order);
		d.read< int >(pt_node_mes_done);
		d.read< int >(pt_node_steps_done);
		d.read< int >(pt_accepted);
#ifdef MCL_HISTOGRAM
		d.read< int >(histogram_up);
		d.read< int >(histogram_down);
#endif
	}
	else {
		pt_moves = 0;
		for (int i=1;i<world_size;++i) {
			pt_node_task.push_back(-1);
			pt_node_order.push_back(-1); 
			pt_node_mes_done.push_back(0);
			pt_node_steps_done.push_back(0);
#ifdef MCL_HISTOGRAM
			histogram_up.push_back(0);
			histogram_down.push_back(0);
#endif
		}
		for (int i=1;i<world_size-1;++i)
			pt_accepted.push_back(0);
	}
	d.close();
	for (uint i = 0; i < taskfiles.size(); i ++){
		parser tp(taskfiles[i]);
		one_task new_task;
		if (tp.defined("SWEEPS")) 
			new_task.n_steps=tp.return_value_of<int>("SWEEPS");
		if (tp.defined("n_steps")) 
			new_task.n_steps=tp.return_value_of<int>("n_steps");
		if (i<tasks.size()) {
			if (new_task.n_steps  > tasks[i].n_steps) tasks[i].is_done=0;
			if (new_task.n_steps != tasks[i].n_steps) tasks[i].n_steps=new_task.n_steps;
		}
		else  {
			new_task.task_id = i;
			new_task.is_done = 0;
			new_task.run_counter = 0;
			tasks.push_back(new_task);
		}
	}
	M_report();
}

void runner_pt :: M_end_of_run()
{
	for (int node=1;node<world_size;++node) {
		if (pt_node_waits_for_global_update[node-1]) {
			M_send_action(A_EXIT, node);
			int node_status;
			MPI_Status stat;
			MPI_Recv(&node_status, 1, MPI_INT, node, T_STATUS, MPI_COMM_WORLD, &stat);
			one_task act;
			int task_comm_size = sizeof(one_task) / sizeof(char);
			MPI_Recv((char*) &act, task_comm_size, MPI_CHAR, node, T_STATUS, MPI_COMM_WORLD, &stat);
			pt_node_steps_done[node-1]=act.steps_done;
			pt_node_mes_done[node-1]=act.mes_done;
		}
	}
	bool need_restart = false;
	for(uint i = 0; i < tasks.size(); i ++)
		if(tasks[i].is_done == 0) need_restart=true;
	if(need_restart) {
		std::string rfilename=jobfile+".restart";
		std::ofstream rfile(rfilename.c_str());
		rfile << "restart me\n";
		rfile.close();
		(*STATUS) << my_rank << ": Restart needed" << "\n";
	}
#ifdef MCL_HISTOGRAM
	else {
		stringstream ss;
		for (size_t i = 0; i < histogram_up.size(); ++i) {
			ss << (double) histogram_up[i]/(histogram_up[i] + histogram_down[i]) << " ";
		}
		string flowfile = jobfile + ".flow";
		ofstream flowf(flowfile.c_str());
		flowf << ss.str();
		flowf.close();
	}
#endif
	M_report();
	M_write();
	MPI_Finalize();
	(*STATUS) << my_rank << ": MPI finalized" << "\n";
	exit(0);
}

void runner_pt :: M_report() 
{
	if (0) {
		for(uint i = 0; i < pt_node_steps_done.size(); i ++) {
		(*STATUS) 
				<< i+1 << "\t"
				<< pt_node_steps_done[i] << "\t"
				<< pt_node_mes_done[i] << "\t"
				<< "\n";
		}
		(*STATUS) << "\n";
	}
}


void runner_pt :: M_report_acc_ratio() {
	std::ofstream afile(acceptfile.c_str(),std::ios::out|std::ios::app);
	afile << pt_moves << " ";
	for(uint i = 0; i < pt_accepted.size(); i++)
		afile<< double(pt_accepted[i])/double(pt_moves) << " ";
	afile << "\n";
	afile.close();
}
#ifdef MCL_POS
void runner_pt::M_report_pt_positions() {
	std::ofstream afile(positionsfile.c_str(), std::ios::out | std::ios::app);
	afile << pt_position_order.str();
	afile.close();
	pt_position_order.str("");
}
#endif

void runner_pt :: M_global_update()
{
	//(*STATUS) << my_rank << ": start global update"<< "\n";
#ifdef MCL_POS
	/*	std::ofstream afile(positionsfile.c_str(),std::ios::out|std::ios::app);
		for (uint i = 0; i < pt_node_order.size(); ++i)
			afile << pt_node_order[i] << " ";
		afile << "\n";
		afile.close();*/
		for (uint i = 0; i < pt_node_order.size(); ++i)
			pt_position_order << pt_node_order[i] << " ";
		pt_position_order << "\n";
#endif

	++pt_moves;

#ifdef MCL_PT_STAGGERED
//staggered version
	for(int i = 0; i < world_size-2 - (((world_size%2) && (pt_moves%2)) ? 1 : 0); i+=2) {
		int a=i;
		int b=i+1;
		if (pt_moves%2) {
			a=i+1;
			b=i+2;
		}
#else
//original version
	for(int i = 0; i < world_size-2; i ++) {
		int a=i;
		int b=i+1;
#endif
		MPI_Send(&a, 1, MPI_INT, pt_node_order[b]+1, T_PARTNER, MPI_COMM_WORLD);
		MPI_Send(&b  , 1, MPI_INT, pt_node_order[a]+1, T_PARTNER, MPI_COMM_WORLD);
		//cerr<<"Send call to "<<pt_node_order[b]+1<<" and " << pt_node_order[a]+1<<"\n";
		MPI_Status stat;
		double W1,W2;
		MPI_Recv(&W1, 1, MPI_DOUBLE, pt_node_order[b]+1, T_WEIGHT, MPI_COMM_WORLD, &stat);
		MPI_Recv(&W2, 1, MPI_DOUBLE, pt_node_order[a]+1, T_WEIGHT, MPI_COMM_WORLD, &stat);
		//cerr<<"Recieved weights"<<"\n";
		if(pt_rng->d()<exp(W1+W2)) {
                        //cout << a << "," << b << "  ";
			int tmp = pt_node_order[a];
			pt_node_order[a] = pt_node_order[b];
			pt_node_order[b] = tmp;
			++pt_accepted[a];
		}
		//cerr<<"Send new positions"<<"\n";
		MPI_Send(&b, 1, MPI_INT, pt_node_order[b]+1, T_PARTNER, MPI_COMM_WORLD);
		MPI_Send(&a, 1, MPI_INT, pt_node_order[a]+1, T_PARTNER, MPI_COMM_WORLD);
		//cerr<<"Sending done"<<"\n";
	}
	//cout << endl;
	int tmp=-1;
	for(int i = 1; i < world_size; i ++) {
		MPI_Send(&tmp, 1, MPI_INT, i, T_PARTNER, MPI_COMM_WORLD);
#ifdef MCL_HISTOGRAM
		int label, pos;
		MPI_Status stat;
		MPI_Recv(&pos, 1, MPI_INT, i, T_POS, MPI_COMM_WORLD, &stat);
		MPI_Recv(&label, 1, MPI_INT, i, T_LABEL, MPI_COMM_WORLD, &stat);
		if (label == 1)
			histogram_up[pos]++;
		else if (label == -1)
			histogram_down[pos]++;
#endif
	}
#ifdef MCL_GLOBAL_ACCEPTANCE_RATE
	M_report_acc_ratio();
#endif
	//(*STATUS) << my_rank << ": did global update"<< "\n";
}


void runner_pt :: global_update()
{
	int ini_pos=my_task.pt_pos;
	bool finished = false;
	while(!finished) {
		MPI_Status stat;
		int partner;
		MPI_Recv(&partner, 1, MPI_INT, MASTER, T_PARTNER, MPI_COMM_WORLD, &stat);
		if (partner == -1) finished = true;
		else {
			double weight = (*sys).get_weight(partner);
			MPI_Send(&weight, 1, MPI_DOUBLE, MASTER, T_WEIGHT, MPI_COMM_WORLD);
			int new_pos;
			MPI_Recv(&new_pos, 1, MPI_INT, MASTER, T_PARTNER, MPI_COMM_WORLD, &stat);
			if(new_pos != my_task.pt_pos) {
				my_task.pt_pos = new_pos;
				(*sys).change_parameter(my_task.pt_pos);
			}
		}
	}
	if (my_task.pt_pos!=ini_pos) (*sys).change_to(my_task.pt_pos);
#ifdef MCL_HISTOGRAM
	int label = (*sys).get_label();
	MPI_Send(&my_task.pt_pos, 1, MPI_INT, MASTER, T_POS, MPI_COMM_WORLD);
	MPI_Send(&label, 1, MPI_INT, MASTER, T_LABEL, MPI_COMM_WORLD);
#endif
}

void runner_pt :: what_is_next(int status)
{
	MPI_Send(&status, 1, MPI_INT, MASTER, T_STATUS, MPI_COMM_WORLD);
	if (status==S_CHECKPOINT) {
		int task_comm_size = sizeof(one_task) / sizeof(char);
		MPI_Send((char*) &my_task, task_comm_size, MPI_CHAR, 0, T_STATUS, MPI_COMM_WORLD);
	}		
	if (status==S_IDLE) {
		int new_action = recv_action();
		if (new_action == A_EXIT) end_of_run();
		MPI_Status stat;
		int task_comm_size = sizeof(one_task) / sizeof(char);
		MPI_Recv((char*) &my_task, task_comm_size, MPI_CHAR, 0, T_NEW_JOB, MPI_COMM_WORLD, &stat);
		std::string taskfile = taskfiles[my_task.task_id];
		stringstream tb; tb<<"task"<<my_task.task_id+1;
		parser cfg(taskfile);
		my_taskdir = cfg.value_or_default("taskdir",tb.str());
		std::stringstream rb; rb<<my_taskdir<<"/run"<<my_rank<<".";
		my_rundir = rb.str();
		run();
	}
	if(status==S_GLOBAL_UPDATE) {
		//(*STATUS) << my_rank << ": want global update"<< "\n";
		int new_action = recv_action();
		if (new_action == A_EXIT) {
			//(*STATUS) << my_rank << ": global update was canceled"<< "\n";
			checkpointing();
			end_of_run();
		}
		else {
			//(*STATUS) << my_rank << ": got global update"<< "\n";
			//cout << my_rank <<" "<< my_task.steps_done<<"\n";
			global_update();
		}
	}
	if(status==S_BUSY) {
		int new_action = recv_action();
		if(new_action == A_EXIT) end_of_run();
	}
	if (status==S_FINISHED) {	
		int task_comm_size = sizeof(one_task) / sizeof(char);
		MPI_Send((char*) &my_task, task_comm_size, MPI_CHAR, 0, T_STATUS, MPI_COMM_WORLD);
		int new_action = recv_action();
		if(new_action == A_PROCESS_DATA_NEW_JOB) {merge_measurements();what_is_next(S_IDLE);}
		else what_is_next(S_IDLE);
	}
}

int runner_pt :: recv_action()
{
	MPI_Status stat;
	int new_action;
	MPI_Recv(&new_action, 1, MPI_INT, MASTER, T_ACTION, MPI_COMM_WORLD, &stat);
	return new_action;
}

void runner_pt :: run()
{
	delete sys;
	string taskfile = taskfiles[ my_task.task_id ];
	sys = new mc_pt(taskfile);
	if ((*sys).read(my_rundir)) {
		(*STATUS) << my_rank << ": L " << my_rundir  << "\n";
		for(int p=1;p<world_size;p++) {
			stringstream b;b<<my_rundir<<"para"<<p<<".";
			(*sys).measure[p-1].read(b.str());
		}
		(*sys).change_to(my_task.pt_pos);
	}
	else {
		(*STATUS) << my_rank << ": I " << my_rundir << "\n";
		(*sys).change_to(my_task.pt_pos);
		(*sys).init();
		checkpointing();
	}
	if((*sys).request_global_update()) what_is_next(S_GLOBAL_UPDATE);
	else what_is_next(S_BUSY);
	while(my_task.mes_done < my_task.n_steps) {	
		my_task.steps_done ++;
		(*sys).do_update();
		if((*sys).is_thermalized()) {
			my_task.mes_done ++;
			(*sys).do_measurement();
		}
		if((*sys).request_global_update()) what_is_next(S_GLOBAL_UPDATE);
		if(is_chkpt_time() || time_is_up()) {
			checkpointing();
			what_is_next(S_BUSY);
		}
	}
	(*STATUS) << my_rank << ": F " << my_rundir << "\n";
	checkpointing();
	what_is_next(S_FINISHED);	
}

void runner_pt :: checkpointing()
{
	(*sys).write(my_rundir);
	for(int p=1;p<world_size;p++) {
		std::stringstream b;b<<my_rundir<<"para"<<p<<".";
		(*sys).measure[p-1].write(b.str());
	}
	what_is_next(S_CHECKPOINT);
	(*STATUS) << my_rank << ": C " << my_rundir << "\n";
}

void runner_pt :: merge_measurements()
{
	bool mergeall=1;
	for (int p=1;p<world_size;++p) {
                if (mergeall) (*sys).measure[p-1].clear();
		for (int r=1;r<world_size;++r)
			if (mergeall || my_task.run_counter!=r) {
				std::stringstream b;
				b<<my_taskdir<<"/run"<<r<<".para"<< p<<".";
				(*sys).measure[p-1].merge(b.str());				
			}
		std::stringstream b;
		if (p<10) b<<my_taskdir<<".para000"<<p<<".out";
                else if (p<100) b<<my_taskdir<<".para00"<<p<<".out";
                else if (p<1000) b<<my_taskdir<<".para0"<<p<<".out";
                else b<<my_taskdir<<".para"<<p<<".out";
		(*sys).write_output(b.str(),p-1);
		//		tb<<my_taskdir<<".out";
		//		(*sys).write_output(tb.str(),p-1);
		(*sys).measure[p-1].clear(); //otherwise, we might run out of memory
	}
	(*STATUS) << my_rank << ": M " << my_taskdir << "\n";
}

void runner_pt :: end_of_run()
{
	MPI_Finalize();
	(*STATUS) << my_rank << ": MPI finalized" << "\n";
	exit(0);
}
