#include <iostream>
#include <stdio.h>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <string.h>
#include <map>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <regex>
#include <signal.h>
#include <stdlib.h>
#include <time.h>
#define NORMAL_PIPE 0
#define NUMBER_PIPE 1
#define EXCLAM_NUMBER_PIPE 2
#define REDIRECTION 3
#define SERV_HOST_ADDR "localhost" 
#define SERV_TCP_PORT 1000
#define MAXLINE 512
using namespace std;


// struct definition
struct User{
	int ID;
	string nickname;
	string IP;
	string port;
};
struct Pipe{
	int read_fd;
	int write_fd;
	Pipe(int *fd){
		read_fd = fd[0];
		write_fd = fd[1];
	}
};
// global variable
map<int,User> mapUser; //user info map by fd
vector<string> vecUserNames; // to check if user name has beed used
bool user_list[31]; // to know if ID exists
int fd_table[31]; // use ID to find corresponding fd 
int instr_cnt[31]; // for number pipe
vector< map<int,Pipe> > NP_sender_map; // for number pipe
vector< map<int,Pipe> > NP_receiver_map; // for number pipe
vector< vector<Pipe> > user_pipe_table; // for user pipe
bool user_pipe_bool[31][31]; // to know if user pipe exists
vector< map<string,string> > env_map; // environment variables map for each user
/************Function********************************/
void initialize(){
	// initialize user_list
	user_list[0] = true;
	for(int i = 1;i < 31;i++){
		user_list[i] = false;
	}
	// initialize fd_table
	for(int i = 0;i < 31;i++){
		fd_table[i] = -1;
	}
	// initialize user_pipe_bool
	for(int i = 0;i < 31;i++){
		vector<Pipe> vP;
		user_pipe_table.push_back(vP);
		for(int j = 0;j < 31;j++){
			user_pipe_bool[i][j] = false;
			int fd[2] = {-1,-1};
			Pipe temp(fd);
			user_pipe_table[i].push_back(temp);
		}
	}
	// initialize NP_sender/receiver
	for(int i = 0;i < 31;i++){
		map<int,Pipe> temp;
		NP_sender_map.push_back(temp);
		NP_receiver_map.push_back(temp);
	}
	// initialize instr_cnt
	for(int i = 0;i < 31;i++){
		instr_cnt[i] = 0;
	}
	// initialize env_map
	for(int i = 0;i< 31;i++){
		map<string,string> env_temp;
		env_temp.insert(pair<string,string>("PATH","bin:."));
		env_map.push_back(env_temp);
	}
	// initialize vecUserNames
	for(int i = 0;i < 31;i++){
		string t = "(no name)";
		vecUserNames.push_back(t);
	}
}
vector<string> split(const string& str, const string delim){
	vector<string> res;
	//cerr << str.length() << endl;
	char *strs = new char[str.length() + 1]; 
	strcpy(strs, str.c_str()); 
 
	char *d = new char[delim.length() + 1];
	strcpy(d, delim.c_str());
 
	char *p = strtok(strs, d);
	while(p){
		string s = p; 
		res.push_back(s);
		p = strtok(NULL, d);
	}
	return res;
}
int exec(string command, int user_id){
	vector<string> split_command;
	char **argv;
	split_command = split(command," ");
		
	// create argv
	argv = new char*[split_command.size()+1];
	for(int i=0;i<split_command.size();i++){
		argv[i] = new char[split_command[i].size()];
	} 
	for(int i=0;i<split_command.size();i++){
		argv[i] = (char*)split_command[i].c_str();
	}
	argv[split_command.size()] = NULL;
	//printf("argv: %s\n",argv[0]);
	map<string,string>::iterator iter;
	iter = env_map[user_id].find("PATH");
	string path_str = iter->second;
	vector<string> all_path  = split(path_str, ":");
	for(int i = 0;i < all_path.size();i++){
		string path = all_path[i] + "/" + split_command[0];
		try{
			//cerr << "try " << path << endl;
			execv(path.c_str(),argv);
		}
		catch(exception &anyException){}
	}
	//execvp(split_command[0].c_str(),argv);

	// should not do this part if execvp succeeds
	cout << "Unknown command: [" << split_command[0] << "]." << endl;
	exit(0);
}
int find_command_type(string command){
	// 
	int num = 0;
	if(split(command,">").size() >= 2){
		try{
			cerr << "num =" << num << endl;
			num = stoi(split(command,">")[1],nullptr,10);
		}
		catch(exception &anyException){}
		if(num == 0)
			return REDIRECTION;
	}
	//
	vector<string> pipe_commands = split(command,"|");
	string last_cmd = pipe_commands[pipe_commands.size()-1];
	num = 0;
	try{
		num = stoi(last_cmd,nullptr,10);
	}
	catch(exception &anyException){}
	if(num > 0)
		return NUMBER_PIPE;
	//
	vector<string> exclam_commands = split(command,"!");
	if(exclam_commands.size() >= 2)
		return EXCLAM_NUMBER_PIPE;
	//
	return NORMAL_PIPE;
}
void childHandler(int signo){
	int status;
	while(waitpid(-1,&status,WNOHANG) > 0){
		// do noting
	}
}
string process_cstr(char* cstr){
	string temp;
	int len = strlen(cstr);
	cerr << len << endl;
	// -2 for delete \r\n
	for(int i = 0;i < len-2;i++){
		temp.push_back(cstr[i]);
	}
	return temp;
}
void shell(int sender_fd, vector<int> &user_fds, char* msg, fd_set &read_fds, int &max_fd){

	// Variable declartion
	
	cerr << "len: " << strlen(msg) << endl;
	string command(msg);
	//command.pop_back();command.pop_back();
	if(command[command.length()-1] == '\n' || command[command.length()-1] == '\r')
		command.pop_back();
	if(command[command.length()-1] == '\n' || command[command.length()-1] == '\r')
		command.pop_back();
	cerr << "command: " << command << endl;
	// Save STDOUT/STDERR
	int stdout_fd = dup(STDOUT_FILENO);
	int stderr_fd = dup(STDERR_FILENO);
	// redirect stdout to sender_fd
	dup2(sender_fd, STDOUT_FILENO);
	// get user id
	map<int, User>::iterator u_iter;
	u_iter = mapUser.find(sender_fd);
	int user_id = u_iter->second.ID; 
	// 
	if(command.compare(0,4,"exit") == 0){

		FD_CLR(sender_fd, &read_fds);
		shutdown(sender_fd, SHUT_RDWR);
		close(sender_fd);
		
		// clear user_fds 
		for(int i = 0;i < user_fds.size();i++){
			if(user_fds[i] == sender_fd){
				user_fds.erase(user_fds.begin()+i);
				break;
			}
		}
		cerr << "clear user_fds ok" << endl;
		// find maxfd
		if(max_fd == sender_fd){
			int m = -1;
			for(int i = 0;i < user_fds.size();i++){
				if(user_fds[i] > m)
					m = user_fds[i];
			}
			max_fd = m;
		}
		cerr << "find maxfd ok" << endl;
		// clear user list
		user_list[user_id] = false;
		cerr << "clear user_list ok" << endl;
		// clear fd_table
		fd_table[user_id] = -1;
		cerr << "clear fd_table ok" << endl;
		// clear vecUserNames
		vecUserNames[user_id] = "(no name)";
		cerr << "clear vecUserNames ok" << endl;
		// clear instr_cnt
		instr_cnt[user_id] = 0;
		cerr << "clear instr_cnt ok" << endl;
		// clear NP_sender/receiver map
		NP_sender_map[user_id].clear();
		NP_receiver_map[user_id].clear();
		cerr << "clear NP sender/receiver ok" << endl;
		// broadcast msg
		string exit_msg;
		exit_msg += "*** User '";
		exit_msg += u_iter->second.nickname;
		exit_msg += "' left. ***\n";
		for(int i = 0;i < user_fds.size();i++){
			write(user_fds[i], exit_msg.c_str(), exit_msg.length());
		}
		// clear user pipe
		for(int i = 0;i < 31;i++){
			user_pipe_bool[i][user_id] = false;
		}
		
		// clear env_map
		map<string,string> env_temp;
		env_temp.insert(pair<string,string>("PATH","bin:."));
		env_map[user_id] = env_temp;
		// erase user info in mapUser
		mapUser.erase(u_iter);
		cerr << "erase mapUser ok" << endl;
		//
		return;
	}
	else if(command == ""){
		cout << "% ";
		return;
	}
	/************Environment setting*****************/
	vector<string> split_command;
	split_command = split(command," |");
	// handle setenv
	if(split_command[0] == "setenv"){
		// if(setenv(split_command[1].c_str(),split_command[2].c_str(),1) < 0){
		// 	cout << "set environment error:" << split_command[1].c_str() << endl;
		// }
		map<string,string>::iterator iter;
		iter = env_map[user_id].find(split_command[1]);
		if(iter != env_map[user_id].end()){
			iter->second = split_command[2];
		}
		cout << "% ";
		return;
	}
	// handle printenv
	else if(split_command[0] == "printenv"){
		// try{
		// 	string env = getenv(split_command[1].c_str());
		// 	cout << env << endl;
		// }
		// catch(exception &anyException){}
		map<string,string>::iterator iter;
		iter = env_map[user_id].find(split_command[1]);
		if(iter != env_map[user_id].end()){
			cout << iter->second << endl;
		}
		cout << "% ";
		return;
	}
	else{}
	/************User communication***********************/
	if(split_command[0] == "yell"){
		// get user in mapUser by fd
		map<int, User>::iterator iter;
		iter = mapUser.find(sender_fd);
		// process yell content
		string yell_content;
		yell_content += "*** ";
		yell_content += iter->second.nickname;
		yell_content += " yelled ";
		yell_content += "***: ";
		for(int i = 5;i < command.length();i++){
			yell_content.push_back(command[i]);
		}
		yell_content.push_back('\n');
		for(int i = 0;i < user_fds.size();i++){
			cerr << "Send message " << yell_content << "to" << user_fds[i] << endl;
			write(user_fds[i], yell_content.c_str(), strlen(yell_content.c_str()));
		}
		cout << "% ";
		return;
	}
	else if(split_command[0] == "name"){
		// get user in mapUser by fd
		map<int, User>::iterator iter;
		iter = mapUser.find(sender_fd);
		// check if name already exists
		string name = split_command[1];
		for(int i = 0;i < vecUserNames.size();i++){
			cerr << "checking: " << name << " != " << vecUserNames[i] << endl; 
			if(name == vecUserNames[i]){
				cout << "*** User '" << name << "' already exists. ***" << endl;
				cout << "% ";
				return;
			}
		}
		// update vecUserNames
		vecUserNames[iter->second.ID] = name;
		// rename user
		iter->second.nickname = name;
		// broadcast message
		string content;
		content += "*** User from ";
		content += iter->second.IP;
		content += ":";
		content += iter->second.port;
		content += " is named '";
		content += iter->second.nickname;
		content += "'. ***\n";
		for(int i = 0;i < user_fds.size();i++){
			cerr << "Send message " << content << "to" << user_fds[i] << endl;
			write(user_fds[i], content.c_str(), strlen(content.c_str()));
		}
		cout << "% ";
		return;
	}
	else if(split_command[0] == "who"){
		// get user in mapUser by fd
		map<int, User>::iterator iter;
		iter = mapUser.find(sender_fd);
		// print user info
		cout << "<ID>\t<nickname>\t<IP:port>\t<indicate me>" << endl;
		for(int i = 1;i <= 30;i++){
			if(fd_table[i] != -1){
				iter = mapUser.find(fd_table[i]);
				User u = iter->second;
				cout << i << '\t' << u.nickname << '\t' << u.IP+":"+u.port << '\t';
				if(fd_table[i] == sender_fd){
					cout << "<-me";
				}
				cout << endl;
			}
		}
		cout << "% ";
		return;
	}
	else if(split_command[0] == "tell"){
		// get user in mapUser by fd
		map<int, User>::iterator iter;
		iter = mapUser.find(sender_fd);
		// get uid from command
		int uid = atoi(split_command[1].c_str());
		// get message from command
		string message;
		message += "*** ";
		message += iter->second.nickname;
		message += " told you ***: ";
		for(int i = 2;i < split_command.size()-1;i++){
			message += split_command[i];
			message += " ";
		}
		message += split_command[split_command.size()-1];
		message += "\n";
		if(user_list[uid] == true){
			// find fd by user_id in fd_table
			int fd = fd_table[uid];
			// send msg to that user
			write(fd, message.c_str(), message.length());
		}
		else{
			cout << "*** Error: user #" << uid << " does not exist yet. ***" << endl;
		}
		cout << "% ";
		return;
	}
	/************Parse command****************************/
	int line_type = find_command_type(command);
	cerr << "line_type: " << line_type << endl; 
	int num_pipe_cnt = -1;
	string file_name;
	int file_fd;
	vector<string> pipe_commands;
	if(line_type == NUMBER_PIPE){
		vector<string> temp = split(command,"|");
		string num_str = temp[temp.size()-1];
		num_pipe_cnt = stoi(num_str,nullptr,10);
		pipe_commands = split(command,"|");
		pipe_commands.pop_back();
	}
	else if(line_type == EXCLAM_NUMBER_PIPE){
		vector<string> temp = split(command,"!");
		string num_str = temp[temp.size()-1];
		num_pipe_cnt = stoi(num_str,nullptr,10);
		command = temp[0];
		pipe_commands = split(command,"|");
	}
	else if(line_type == REDIRECTION){
		vector<string> temp = split(command,"> ");
		file_name = temp[temp.size()-1];
		pipe_commands = split(command,"|");
		pipe_commands[pipe_commands.size()-1] = split(pipe_commands[pipe_commands.size()-1],">")[0];
		file_fd = open(file_name.c_str(),O_RDWR|O_TRUNC|O_CREAT,S_IRWXU|S_IRWXG|S_IRWXO);
		//cerr << file_name << endl;
		if(file_fd < 0)
			cerr << "Open file error" << endl; 
	}
	else{ // normal pipe
		num_pipe_cnt = -1;
		file_name = "NONE";
		pipe_commands = split(command,"|");
	}
	/***********Pipe job*******************************/
	pid_t pid;
	int cur_period = 0;
	vector<int> pid_table;
	vector<Pipe> pipe_table;
	int stop_point = 0;
	for(int i = 0;i < pipe_commands.size();i++){	
		int fd[2];
		signal(SIGCHLD, childHandler);
		// not last command
		if(i < pipe_commands.size()-1){
			while(pipe(fd) < 0){
				// do nothing
			}
			cerr << "normal pipe get pipe: " << fd[0] << " & " << fd[1] << endl;
			Pipe temp_pipe(fd);
			pipe_table.push_back(temp_pipe);
		}
		// number pipe's last command
		else if(line_type == NUMBER_PIPE || line_type == EXCLAM_NUMBER_PIPE){
			map<int, Pipe>::iterator iter;
			iter = NP_receiver_map[user_id].find(instr_cnt[user_id]+num_pipe_cnt);
			// if destination has existed
			if(iter != NP_receiver_map[user_id].end()){
				Pipe p = iter->second;
				NP_sender_map[user_id].insert(pair<int,Pipe>(instr_cnt[user_id],p));
			}
			else{
				cerr << "number pipe get pipe\n";
				while(pipe(fd)<0){
					// do nothing
				}
				cerr << "number pipe get pipe: " << fd[0] << " & " << fd[1] << endl;
				Pipe temp_pipe(fd);
				NP_sender_map[user_id].insert(pair<int,Pipe>(instr_cnt[user_id],temp_pipe));
				NP_receiver_map[user_id].insert(pair<int,Pipe>(instr_cnt[user_id]+num_pipe_cnt,temp_pipe));
			}
			
		}
		regex reg("[<][0-9]+");
		smatch m;
		ssub_match sm;
		bool is_pipe_in = false;
		int send_id, recv_id;
		// check if user pipe(input)
		if(regex_search(pipe_commands[i], m, reg)){
			is_pipe_in = true;
			// get first search	result			
		    sm = m[0];
		    string str = sm.str();
		    // get digits after '>'
		    str = split(str,"<")[0];
		    send_id = atoi(str.c_str());
		    cerr << "user pipe from: " << send_id << endl;
		    //
		    map<int, User>::iterator iter,iter2;
			iter = mapUser.find(sender_fd); //receiver
			iter2 = mapUser.find(fd_table[send_id]); //sender
			recv_id = iter->second.ID;
		    // check if sender exists
			if(user_list[send_id] == false){
				string err;
				err += "*** Error: user #";
				err += to_string(send_id);
				err += " does not exist yet. ***";
				cout << err << endl;
				cout << "% ";
				return;
			}
			// if user pipe exists
			if(user_pipe_bool[send_id][recv_id] == true){
				// broadcast msg
				string up_msg;
				up_msg += "*** ";
				up_msg += iter->second.nickname;
				up_msg += " (#";up_msg += to_string(recv_id);up_msg += ")";
				up_msg += " just received from ";
				up_msg += iter2->second.nickname;
				up_msg += " (#";up_msg += to_string(send_id);up_msg += ")";
				up_msg += " by '";
				up_msg += command;
				up_msg += "' ***\n";
				for(int i = 1;i <= 30;i++){
					if(user_list[i] == true){
						write(fd_table[i],up_msg.c_str(),up_msg.length());
					}
				}
			}
			// if user pipe doesn't exists
			else{
				// print error msg
				cout << "*** Error: the pipe #";
				cout << send_id;
				cout << "->#";
				cout << recv_id;
				cout << " does not exist yet. ***\n";
				cout << "% ";
				return;
			}
		}
		// check if user pipe(output)
		reg = "[>][0-9]+";
		if(regex_search(pipe_commands[i], m, reg)){
			// get first search	result			
		    sm = m[0];
		    string str = sm.str();
		    // get digits after '>'
		    str = split(str,">")[0];
		    recv_id = atoi(str.c_str());
		    cerr << "user pipe to: " << recv_id << endl;
		    // get Pipe
		    int fd[2];
		    while(pipe(fd)<0){
				// do nothing
			}
			Pipe temp_pipe(fd);
			map<int, User>::iterator iter,iter2;
			iter = mapUser.find(sender_fd);
			send_id = iter->second.ID;
			// if receiver does not exists, print error
			if(user_list[recv_id] == false){
				string err;
				err += "*** Error: user #";
				err += to_string(recv_id);
				err += " does not exist yet. ***";
				cout << err << endl;
				cout << "% ";
				return;
			}
			// if user pipe does not exist
			if(user_pipe_bool[send_id][recv_id] == false){
				// record pipe number into user_pipe_table
				user_pipe_table[send_id][recv_id] = temp_pipe;
				cerr << "get user pipe" << fd[0] << " & " << fd[1] << endl;
				user_pipe_bool[send_id][recv_id] = true;
				// broadcast msg
				string up_msg;
				up_msg += "*** ";
				up_msg += iter->second.nickname;
				up_msg += " (#";up_msg += to_string(send_id);up_msg += ")";
				up_msg += " just piped '";
				up_msg += command;
				up_msg += "' to ";
				iter2 = mapUser.find(fd_table[recv_id]);
				up_msg += iter2->second.nickname;
				up_msg += " (#";up_msg += to_string(recv_id);up_msg += ") ***\n";
				for(int i = 1;i <= 30;i++){
					if(user_list[i] == true){
						write(fd_table[i],up_msg.c_str(),up_msg.length());
					}
				}
			}
			// if pipe exists, then print error msg
			else{
				cout << "*** Error: the pipe #";
				cout << send_id;
				cout << "->#";
				cout << recv_id;
				cout << " already exists. ***\n";
				cout << "% ";
				return;
			}
		    

		}
		
		pid = fork();
		if(pid > 0){ // parent
			pid_table.push_back(pid);
			if(line_type == REDIRECTION && i == pipe_commands.size() - 1){
				close(file_fd);
			}
			// if it's first command
			if(i == 0){
				// after number pipe receiver is done, release pipe
				map<int, Pipe>::iterator iter;
				iter = NP_receiver_map[user_id].find(instr_cnt[user_id]);
				if(iter != NP_receiver_map[user_id].end()){
					Pipe p = iter->second;
					close(p.read_fd);
					close(p.write_fd);
				}
				if(is_pipe_in == true){
					// find recv_id/send_id
					reg = "[<][0-9]+";
					regex_search(pipe_commands[i], m, reg);
					sm = m[0];
				    string str = sm.str();
				    // get digits after '<'
				    str = split(str,"<")[0];
				    send_id = atoi(str.c_str());
				    cerr << "user pipe from: " << send_id << endl;
				    //
				    map<int, User>::iterator iter,iter2;
					iter = mapUser.find(sender_fd); //receiver
					iter2 = mapUser.find(fd_table[send_id]); //sender
					recv_id = iter->second.ID;
					Pipe p = user_pipe_table[send_id][recv_id];
					close(p.read_fd);
					close(p.write_fd);
					cerr << send_id << " & " << recv_id << endl;
					user_pipe_bool[send_id][recv_id] = false;
					is_pipe_in = false;
				}
			}
			// not first command, close previous pipe
			regex reg_i("[<][0-9]+");
			regex reg_o("[>][0-9]+");
			
			// if user pipe(input)
			
			if(i >= 1 && (line_type != NUMBER_PIPE && line_type != EXCLAM_NUMBER_PIPE)){
				smatch m;
				if(regex_search(command, m, regex("[>][0-9]+")) < 0){
					cerr << "close pipe: " << pipe_table[i-1].read_fd << " & " << pipe_table[i-1].write_fd << endl;
					close(pipe_table[i-1].read_fd);
					close(pipe_table[i-1].write_fd);
				}	
			}					
		}
		else if(pid == 0){ // child
			if(i > 0){
				dup2(pipe_table[i-1].read_fd,STDIN_FILENO);
			}
			else{ //i == 0, first command
				map<int, Pipe>::iterator iter;
				iter = NP_receiver_map[user_id].find(instr_cnt[user_id]);
				if(iter != NP_receiver_map[user_id].end()){
					cerr << "number pipe get msg\n";
					Pipe p = iter->second;
					close(p.write_fd);
					dup2(p.read_fd,STDIN_FILENO);
					close(p.read_fd);
				}
			}
			if(i < pipe_commands.size()-1){
				dup2(pipe_table[i].write_fd,STDOUT_FILENO);
			}
			else{ // last command
				if(line_type == REDIRECTION){
					//cerr << "ok\n";
					dup2(file_fd, STDOUT_FILENO);
					close(file_fd);
				}
				if(line_type == NUMBER_PIPE){
					cerr << "number pipe send msg\n";
					map<int, Pipe>::iterator iter;
					iter = NP_sender_map[user_id].find(instr_cnt[user_id]);
					Pipe p = iter->second;
					close(p.read_fd);
					dup2(p.write_fd, STDOUT_FILENO);
					close(p.write_fd);
				}
				if(line_type == EXCLAM_NUMBER_PIPE){
					map<int, Pipe>::iterator iter;
					iter = NP_sender_map[user_id].find(instr_cnt[user_id]);
					Pipe p = iter->second;
					close(p.read_fd);
					dup2(p.write_fd, STDOUT_FILENO);
					dup2(p.write_fd, STDERR_FILENO);
					close(p.write_fd);
				}
			}
			for(int j=0;j<pipe_table.size();j++){
				close(pipe_table[j].read_fd);
				close(pipe_table[j].write_fd);
			}
			// user pipe setting
			regex reg_i("[<][0-9]+");
			regex reg_o("[>][0-9]+");
			smatch m;
			ssub_match sm;
			// if user pipe(input)
			if(regex_search(pipe_commands[i], m, reg_i)){
				// get send_id / recv_id	
			    sm = m[0];
			    string str = sm.str();
			    // get digits after '<'
			    str = split(str,"<")[0];
			    send_id = atoi(str.c_str());
			    //
			    map<int, User>::iterator iter;
				iter = mapUser.find(sender_fd); //receiver
				recv_id = iter->second.ID;

				// replace "<2" to ""
				pipe_commands[i] = regex_replace(pipe_commands[i], reg_i, "");
				cerr << "Replace it to be: " <<  pipe_commands[i] << endl;
				// stdin redirect from user pipe
				Pipe p = user_pipe_table[send_id][recv_id];
				close(p.write_fd);
				dup2(p.read_fd ,STDIN_FILENO);
				close(p.read_fd);
			}
			// if user pipe(output), replace ">2" to ""
			if(regex_search(pipe_commands[i], m, reg_o)){
				// get send_id / recv_id	 
				sm = m[0];
			    string str = sm.str();
			    // get digits after '>'
			    str = split(str,">")[0];
			    recv_id = atoi(str.c_str());
				map<int, User>::iterator iter;
				iter = mapUser.find(sender_fd);
				send_id = iter->second.ID;
				// replace ">2" to ""
				pipe_commands[i] = regex_replace(pipe_commands[i], reg_o, "");
				cerr << "Replace it to be: " <<  pipe_commands[i] << endl;
				// stdout redirect to user pipe
				Pipe p = user_pipe_table[send_id][recv_id];
				close(p.read_fd);
				dup2(p.write_fd ,STDOUT_FILENO);
				close(p.write_fd);
			}
			if(pipe_commands[i].compare(0,10,"removetag0") == 0){
				dup2(sender_fd, STDERR_FILENO);
				close(sender_fd);
			}
			exec(pipe_commands[i],user_id);
		}
		else{// fork error
			//cerr << "gg"<< endl;
			i--;
			pipe_table.pop_back();
			waitpid(-1,NULL,0);
		}
	}// end pipe command for loop

	// release pipe
	for(int i=0;i<pipe_table.size();i++){
		close(pipe_table[i].read_fd);
		close(pipe_table[i].write_fd);
	}
	
	// NEED to releas pipe before WAITPID function!!!!!
	// wait for all commands done for Normal pipe & Redirection
	if(line_type != NUMBER_PIPE && line_type != EXCLAM_NUMBER_PIPE){
		regex reg("[>][0-9]+");
		smatch m;
		// if user pipe(output)
		if(regex_search(command, m, reg) <= 0){
			for(int i=0;i<pid_table.size();i++){
				int status;
				waitpid(pid_table[i],&status,0);
			}
		}
	}
	// prompt
	//cout << "% ";
	write(sender_fd,"% ",2);
	// restore STDOUT/STRERR
	dup2(stdout_fd, STDOUT_FILENO);
	close(stdout_fd);
	// number pipe, not finish in project 2
	instr_cnt[user_id]++;
	cerr << instr_cnt[user_id] << endl;
	return;
}
int main(int argc, char** argv){
	//
	
	//
	cerr << "Server start\n";
	//
	int sock_fd = -1;
	int max_fd;
	fd_set read_fds;
	FD_ZERO(&read_fds);
	vector<int> user_fds;
	struct sockaddr_in serv_addr;
	// initialization
	initialize();
	// fill in the structure "serv_addr"
	bzero((char *) &serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET; //for IPv4
	serv_addr.sin_addr.s_addr = inet_addr("127.0.0.1"); // IP(string)
	
	serv_addr.sin_port = htons(atoi(argv[1])); // Port(integer)
	
	// socket
	sock_fd = socket(AF_INET, SOCK_STREAM, 0);
	// to reuse the port
	int t = 1;
	setsockopt(sock_fd,SOL_SOCKET,SO_REUSEADDR,&t,sizeof(int));
	// register sock_fd into read_fds
	FD_SET(sock_fd, &read_fds);

	if(sock_fd == -1)
		cerr << "Fail to create a socket." << endl;
	// bind
	if(bind(sock_fd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0)
		cerr << "Fail to bind." << endl;
	// listen
	listen(sock_fd,5);
	/***********************Server******************************/
	//Set environment variable PATH
	//if(setenv("PATH","bin:.",1) < 0){ cout << "Initial Path error" << endl;}
	while(1){
		int new_sock_fd;
		struct sockaddr_in cli_addr;
		pid_t client_pid;
		unsigned int client_len = sizeof(cli_addr);
		// deal with 'exit' command
		if(max_fd < sock_fd)
			max_fd = sock_fd;
		
		// CAUTION: need to set read_fds to 0 in each iteration
		FD_ZERO(&read_fds);
		FD_SET(sock_fd, &read_fds);
		for(int i = 0;i < user_fds.size();i++){
			FD_SET(user_fds[i], &read_fds);
		}
		// select stuff
		switch(select(max_fd+1, &read_fds, NULL, NULL, NULL)){
			case -1:
				cerr << "select error" << endl;
				break;
			case 0:
				cerr << "time out" << endl;
				break;
			default:
				// TCP socket
				if(FD_ISSET(sock_fd, &read_fds) > 0){
					new_sock_fd = accept(sock_fd, (struct sockaddr*)&cli_addr, &client_len);
					
					// might wrong, watch out
					if(new_sock_fd > max_fd)
						max_fd = new_sock_fd;
					// register new_sock_fd into user_fds
					user_fds.push_back(new_sock_fd);
					if(new_sock_fd < 0)
						cerr << "server accept error" << endl;
					// register sock_fd into read_fds
					cerr << "Registered " << new_sock_fd << endl;
					FD_SET(new_sock_fd, &read_fds);
					// register new user to mapUser
					User u;
					for(int i = 1;i <= 30; i++){
						if(user_list[i] == false){
							u.ID = i;
							user_list[i] = true;
							break;
						}
					}
					u.nickname = "(no name)";
					u.IP =  inet_ntoa(cli_addr.sin_addr);
					u.port =  to_string(htons(cli_addr.sin_port));
					// cerr << "ID:" << u.ID << endl;
					// cerr << "IP:" << u.IP << endl;
					// cerr << "port:" << u.port << endl;
					mapUser.insert(pair<int,User>(new_sock_fd, u));
					// register fd to fd_table
					fd_table[u.ID] = new_sock_fd;
					// send welcome message
					string wel_msg = "****************************************\n";
					wel_msg += "** Welcome to the information server. **";
					wel_msg += "\n****************************************\n";
					write(new_sock_fd,wel_msg.c_str(),wel_msg.length());
					// produce login message
					string login_msg;
					login_msg += "*** User '(no name)' entered from ";
					login_msg += u.IP;
					login_msg += ":";
					login_msg += u.port;
					login_msg += ". ***\n";
					// broadcast login message
					for(int i = 0;i < user_fds.size();i++){
						write(user_fds[i],login_msg.c_str(),login_msg.length());
					}
					// first prompt
					write(new_sock_fd,"% ",2);
				}
				// user socket
				else{
					for(int i = 0;i<user_fds.size();i++){
						if(FD_ISSET(user_fds[i], &read_fds) > 0){
							cerr << "Get message from " << user_fds[i] << endl;
							// send message from user_fds[i]to others
							char msg[MAXLINE];
							memset(msg,'\0',MAXLINE);
							// check read's return value, if == -1, user disconnected
							if(read(user_fds[i], msg, MAXLINE) == -1){
								cerr << "Read error" << endl;	
							}
							shell(user_fds[i], user_fds, msg, read_fds, max_fd);
							cerr << "end shell" << endl;
						}
					}
				}
		}
	} // end main server while
	return 0;
}// end main funtion
