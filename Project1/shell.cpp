#include <iostream>
#include <stdio.h>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <string.h>
#include <map>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <time.h>
#define ERROE -1
#define NORMAL_PIPE 0
#define NUMBER_PIPE 1
#define EXCLAM_NUMBER_PIPE 2
#define REDIRECTION 3
#define MAX_COMMAND_FD 2000
#define MAX_COMMAND_PIPE 1000
#define MAX_INSTR_FD 100
#define MAX_INSTR_PIPE 50
using namespace std;

struct Pipe{
	int read_fd;
	int write_fd;
	Pipe(int *fd){
		read_fd = fd[0];
		write_fd = fd[1];
	}
};
/************Function********************************/
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
int exec(string command){
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
	execvp(split_command[0].c_str(),argv);

	// should not do this part if execvp succeeds
	cerr << "Unknown command: [" << split_command[0] << "]." << endl;
	exit(0);
}
int find_command_type(string command){
	//
	if(split(command,">").size() >= 2)
		return REDIRECTION;
	//
	vector<string> pipe_commands = split(command,"|");
	string last_cmd = pipe_commands[pipe_commands.size()-1];
	int num = 0;
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
int main(){
	/************Variable Declaration********************/
	string command;
	int instr_cnt = 0;
	map<int,Pipe> NP_sender_map;
	map<int,Pipe> NP_receiver_map;
	map<int, Pipe>::iterator iter;
	/************Set environment variable PATH************/
	if(setenv("PATH","bin:.",1) < 0){ cout << "Initial Path error" << endl;}
	/************Print shell prompt symbol****************/
	cout << "% ";
	while(getline(cin,command)){
		if(command == "exit"){
			exit(0);
		}
		else if(command == ""){
			cout << "% ";
			continue;
		}
		/************Environment setting*****************/
		vector<string> split_command;
		split_command = split(command," |");

		// handle setenv
		if(split_command[0] == "setenv"){
			if(setenv(split_command[1].c_str(),split_command[2].c_str(),1) < 0){
				cout << "set environment error:" << split_command[1].c_str() << endl;
			}
			cout << "% ";
			continue;
		}
		// handle printenv
		else if(split_command[0] == "printenv"){
			try{
				string env = getenv(split_command[1].c_str());
				cout << env << endl;
			}
			catch(exception &anyException){}
			cout << "% ";
			continue;
		}
		else{}
		/************Parse command****************************/
		int line_type = find_command_type(command);
		//cerr << "line_type: " << line_type << endl; 
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
			string num_str = temp[1];
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
		else{
			num_pipe_cnt = -1;
			file_name = "NONE";
			pipe_commands = split(command,"|");
		}
		/***********Pipe job*******************************/
		int command_fd[MAX_COMMAND_FD];
		int instr_fd[MAX_INSTR_FD];
		pid_t pid;
		int cur_period = 0;
		vector<int> pid_table;
		vector<Pipe> pipe_table;
		int stop_point = 0;
		for(int i = 0;i < pipe_commands.size();i++){			
			int fd[2];
			signal(SIGCHLD, childHandler);
			// if(i < pipe_commands.size()-1){
			// 	while(pipe(fd)<0){
			// 		for(int j=stop_point;j<stop_point+100;j++){
			// 			close(pipe_table[j].read_fd);
			// 			close(pipe_table[j].write_fd);
			// 		}
			// 		stop_point += 100;
			// 	}
			// 	Pipe temp_pipe(fd);
			// 	pipe_table.push_back(temp_pipe);
			// }
			// not last command
			if(i < pipe_commands.size()-1){
				while(pipe(fd) < 0){
					// do nothing
				}
				Pipe temp_pipe(fd);
				pipe_table.push_back(temp_pipe);
			}
			// number pipe's last command
			else if(line_type == NUMBER_PIPE || line_type == EXCLAM_NUMBER_PIPE){
				iter = NP_receiver_map.find(instr_cnt+num_pipe_cnt);
				// if destination has existed
				if(iter != NP_receiver_map.end()){
					Pipe p = iter->second;
					NP_sender_map.insert(pair<int,Pipe>(instr_cnt,p));
				}
				else{
					while(pipe(fd)<0){
						// do nothing
					}
					Pipe temp_pipe(fd);
					NP_sender_map.insert(pair<int,Pipe>(instr_cnt,temp_pipe));
					NP_receiver_map.insert(pair<int,Pipe>(instr_cnt+num_pipe_cnt,temp_pipe));
				}
				
			}
			//cout << fd[0] << " " << fd[1] << endl;
			pid = fork();
			if(pid > 0){ // parent
				pid_table.push_back(pid);
				if(line_type == REDIRECTION && i == pipe_commands.size() - 1){
					close(file_fd);
				}
				// after number pipe receiver is done, release pipe
				if(i == 0){
					iter = NP_receiver_map.find(instr_cnt);
					if(iter != NP_receiver_map.end()){
						Pipe p = iter->second;
						close(p.read_fd);
						close(p.write_fd);
					}
				}
				if(i >= 1){
					close(pipe_table[i-1].read_fd);
					close(pipe_table[i-1].write_fd);
				}						
			}
			else if(pid == 0){ // child
				if(i > 0){
					dup2(pipe_table[i-1].read_fd,STDIN_FILENO);
				}
				else{ //i == 0, first command
					iter = NP_receiver_map.find(instr_cnt);
					if(iter != NP_receiver_map.end()){
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
						iter = NP_sender_map.find(instr_cnt);
						Pipe p = iter->second;
						close(p.read_fd);
						dup2(p.write_fd, STDOUT_FILENO);
						close(p.write_fd);
					}
					if(line_type == EXCLAM_NUMBER_PIPE){
						iter = NP_sender_map.find(instr_cnt);
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
				exec(pipe_commands[i]);
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
			for(int i=0;i<pid_table.size();i++){
				int status;
				waitpid(pid_table[i],&status,0);
			}
		}
		// prompt
		cout << "% ";
		instr_cnt++;
	}// end while
	return 0;
}// end main funtion
