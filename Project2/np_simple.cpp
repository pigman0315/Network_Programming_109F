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
#include <netinet/in.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
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
	cout << "Unknown command: [" << split_command[0] << "]." << endl;
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
int main(int argc, char** argv){

	cerr << "server start\n";
	int sock_fd = 0;
	struct sockaddr_in serv_addr;
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
	if(sock_fd == -1)
		cerr << "Fail to create a socket." << endl;
	// bind
	if(bind(sock_fd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0)
		cerr << "Fail to bind." << endl;
	// listen
	listen(sock_fd,5);
	
	/************Print shell prompt symbol****************/
	while(1){
		int new_sock_fd;
		struct sockaddr_in cli_addr;
		pid_t client_pid;
		unsigned int client_len = sizeof(cli_addr);
		char line_cstr[MAXLINE];
		int line_len;
		// accept
		new_sock_fd = accept(sock_fd, (struct sockaddr*)&cli_addr, &client_len);
		if(new_sock_fd < 0)
			cerr << "server accept error" << endl;
		// after accept a client connection, fork process to deal with it
		client_pid = fork();
		// fork error
		if(client_pid < 0){
			cerr << "Client fork error" << endl;
		}
		// parent
		else if(client_pid > 0){ 
			close(new_sock_fd);
		}
		// child process
		else{
			// Variable declartion
			string command;
			int instr_cnt = 0;
			map<int,Pipe> NP_sender_map;
			map<int,Pipe> NP_receiver_map;
			map<int, Pipe>::iterator iter;
			
			//Set environment variable PATH
			if(setenv("PATH","bin:.",1) < 0){ cout << "Initial Path error" << endl;}
			// redirect stdout & stdin to new_sock_fd
			dup2(new_sock_fd,STDOUT_FILENO);
			dup2(new_sock_fd,STDIN_FILENO);
			
			cout << "% ";
			// read line from client
			while(getline(cin,command)){
				// delete \r\n
				if(command[command.length()-1] == '\n' || command[command.length()-1] == '\r')
					command.pop_back();
				if(command[command.length()-1] == '\n' || command[command.length()-1] == '\r')
					command.pop_back();		

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
						if(i >= 1 && (line_type != NUMBER_PIPE && line_type != EXCLAM_NUMBER_PIPE)){
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
		}// end else clien_pid == 0
	} // end main server while
	return 0;
}// end main funtion
