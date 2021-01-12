#include <iostream>
#include <stdio.h>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <string.h>
#include <map>
#include <unistd.h>
#include <regex>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <array>
#include <boost/bind.hpp>
#include <boost/asio.hpp>
#include <boost/algorithm/string.hpp>
#include <time.h>
using namespace std;
using namespace boost::asio;
io_service io_serv;
class SocksSession : public enable_shared_from_this<SocksSession> {
private:
	enum { CONNECT = 0, BIND = 1 };
	ip::tcp::socket cli_socket, serv_socket;
	ip::tcp::resolver resolver;
	ip::tcp::acceptor _acceptor;
	unsigned char req_msg[4096];
	unsigned char reply_msg[4096];
	unsigned char cli_data[4096];
	unsigned char serv_data[4096];
	string cli_addr, cli_port, serv_addr, serv_port, domain_name;
	int VN, CD;
	bool reply_state, is_socks4a;
	unsigned short data_port;
public:
	// constructor
	SocksSession(ip::tcp::socket socket):
				cli_socket(move(socket)),
				serv_socket(io_serv),
				resolver(io_serv),
				_acceptor(io_serv){
					reply_state = true;
					is_socks4a = false;
					data_port = 1000;
					memset(reply_msg, '\0', sizeof(reply_msg));
					memset(req_msg, '\0', sizeof(req_msg));
					memset(cli_data, '\0', sizeof(cli_data));
					memset(serv_data, '\0', sizeof(serv_data));
				}
	void start() { handle_request(); }
private:
	void handle_request(){
		auto self(shared_from_this());
		// synchronous read
		cli_socket.async_read_some(
			buffer(req_msg, 4096),
			[this, self](boost::system::error_code ec, size_t length) {
				if(!ec){
					// get cli_serv, cli_port
					cli_addr = cli_socket.remote_endpoint().address().to_string();
					cli_port = to_string(cli_socket.remote_endpoint().port());
					// parse request
					VN = int(req_msg[0]);
					CD = int(req_msg[1]);
					// VN should be 4 as SOCKS protocol version
					if(VN != 4){
						cerr << "ERROR: wrong proxy protocol\n";
						cli_socket.close();
						exit(0);
					}
					// serv_port
					short p = int(req_msg[2])*256 + int(req_msg[3]);
					serv_port = to_string(p);
					// serv_addr
					serv_addr = to_string(int(req_msg[4])) + "."+ to_string(int(req_msg[5])) + "."+ to_string(int(req_msg[6])) + "."+ to_string(int(req_msg[7]));
					// check if socks4a
					if(int(req_msg[4])== 0 && int(req_msg[5]) == 0 && int(req_msg[6]) == 0 && int(req_msg[7]) != 0){
						int idx = 8;
						is_socks4a = true;
						if(int(req_msg[idx]) == 0){
							idx++;
						}
						else{
							while(int(req_msg[idx]) != 0){
								idx++;
							}
						}
						while(int(req_msg[idx]) != 0){
							domain_name.push_back(req_msg[idx]);
							idx++;
						}
						//cout << "domain name: " << domain_name << endl;
						//serv_addr = domain_name;
					}
			        //
			        if(CD == 1){
			        	do_connect();
			        }
			        else{
			        	do_bind();
			        }
				}
			});
		
	}
	bool is_block(){
		// do firewall job!
		// permint destination IP
		fstream fin;
		fin.open("socks.conf", ios::in);
		if(!fin){
			cerr << "ERROR: open file failed\n";
		}
		int mode;
		// connection
		if(CD == 1)
			mode = 0;
		// bind
		else
			mode = 1;
		//
		vector<string> dst_part;
		boost::split(dst_part,serv_addr,boost::is_any_of("."));
		string tmp;
		if(mode == 0){
			while(getline(fin,tmp)){
				vector<string> tokens;
				boost::split(tokens,tmp,boost::is_any_of(" "));
				if(tokens[1] == "c"){
					vector<string> ip_part;
					string ip = tokens[2];
					boost::split(ip_part,ip,boost::is_any_of("."));
					int cnt = 0;
					for(int i = 0;i < 4;i++){
						if(ip_part[i] == "*" || ip_part[i] == dst_part[i])
							cnt++;
					}
					if(cnt == 4){
						fin.close();
						return false;
					}
				}
			}
		}
		else{
			while(getline(fin,tmp)){
				vector<string> tokens;
				boost::split(tokens,tmp,boost::is_any_of(" "));
				if(tokens[1] == "b"){
					vector<string> ip_part;
					string ip = tokens[2];
					boost::split(ip_part,ip,boost::is_any_of("."));
					int cnt = 0;
					for(int i = 0;i < 4;i++){
						if(ip_part[i] == "*" || ip_part[i] == dst_part[i])
							cnt++;
					}
					if(cnt == 4){
						fin.close();
						return false;
					}
				}
			}
		}
		fin.close();
		return true;
	}
	void do_bind(){
		auto self(shared_from_this());
		// check firewall rule
		if(is_block()){
			reply_state = false;
			do_reply(BIND);
		}
		// bind a data socket
		srand(time(NULL));
		while(1){
			boost::system::error_code ec;
			data_port = rand()% 5000 + 10000; // range from 10000 ~ 14999
			//cout << "data_port: " << data_port << endl;
			ip::tcp::endpoint ep(ip::tcp::v4(), data_port);
			_acceptor.open(ep.protocol());
			_acceptor.bind(ep,ec);
			if(!ec){
				// successfully bind a socket
				break;
			}
			else{
				_acceptor.close();
				cerr << "ERROR: fail to bind\n";
			}
		}
		_acceptor.listen();
		do_reply(BIND);
		// 
	}
	void do_connect(){
		auto self(shared_from_this());
		// do connection with server
		if(is_socks4a == false){
			ip::tcp::resolver::query query(serv_addr, serv_port);
			resolver.async_resolve(
				query,
				boost::bind(&SocksSession::resolve_handler, self,
	            boost::asio::placeholders::error,
	            boost::asio::placeholders::iterator)
			);
		}
		else{
			ip::tcp::resolver::query query(domain_name, serv_port);
			resolver.async_resolve(
				query,
				boost::bind(&SocksSession::resolve_handler, self,
	            boost::asio::placeholders::error,
	            boost::asio::placeholders::iterator)
			);
		}
		
	}
	void resolve_handler(const boost::system::error_code& ec,
		ip::tcp::resolver::iterator endpoint_iterator)
	{
		auto self(shared_from_this());
		if(!ec){
			ip::tcp::endpoint ep = *endpoint_iterator;
			if(is_socks4a)
				serv_addr = ep.address().to_string();
			//cout << "dest ip: " << ep.address().to_string() << endl;
			serv_socket.async_connect(ep,
				boost::bind(&SocksSession::connect_handler, self,boost::asio::placeholders::error, ++endpoint_iterator));
		}
		else{
			cerr << "ERROR: resolve server.\n";
			reply_state = false;
		}
	}
	void connect_handler(const boost::system::error_code& ec,
		ip::tcp::resolver::iterator endpoint_iterator)
	{
		auto self(shared_from_this());
		if(ec){
			cerr << "ERROR: connect to server.\n";
			reply_state = false;
			
		}
		// check socks.conf and block the connection if not allowed
		if(is_block()){
			reply_state = false;
		}
		do_reply(CONNECT);
	}
	void do_reply(int mode){
		auto self(shared_from_this());
		//
		// CONNECT operation
		if(mode == CONNECT){
			memset(reply_msg, 0, sizeof(unsigned char)*8);
			// 90: request granted, 91: request rejected or failed
			reply_msg[0] = 0;
			if(reply_state == true){
				reply_msg[1] = 90;
			}
			else{
				reply_msg[1] = 91;
			}
			// SOCKS Server Messages
			cout << "<S_IP>: " << cli_addr << endl;
		    cout << "<S_PORT>: " << cli_port << endl;
		    cout << "<D_IP>: " << serv_addr << endl;
		    cout << "<D_PORT>: " << serv_port << endl;
		    if(CD == 1){
		    	cout << "<Command>: CONNECT"  << endl;
		    }
		   	else{
		   		cout << "<Command>: BIND"  << endl;
		   	}
		    // send reply to client & do data transmission
		    boost::system::error_code ec;
		    write(cli_socket, buffer(reply_msg,8), ec);
		    if(!ec && reply_state){
		    	cli_to_serv();
		    	serv_to_cli();
		    	// don't put exit(0) here
		    }
		    if(reply_state == true){
		    	cout << "<Reply>: Accept"  << endl << endl;
		    }
		    else{
		    	cout << "<Reply>: Reject"  << endl << endl;
		    }
		}
		// BIND operation
		else if(mode == BIND){
			memset(reply_msg, 0, sizeof(unsigned char)*8);
			reply_msg[0] = 0;
			if(reply_state == true){
				reply_msg[1] = 90;
			}
			else{
				reply_msg[1] = 91;
			}
			reply_msg[2] = data_port / 256;
			reply_msg[3] = data_port % 256;
			// SOCKS Server Messages
			cout << "<S_IP>: " << cli_addr << endl;
		    cout << "<S_PORT>: " << cli_port << endl;
		    cout << "<D_IP>: " << serv_addr << endl;
		    cout << "<D_PORT>: " << serv_port << endl;
		    if(CD == 1){
		    	cout << "<Command>: CONNECT"  << endl;
		    }
		   	else{
		   		cout << "<Command>: BIND"  << endl;
		   	}
			// send first reply to client
			boost::system::error_code ec;
		    write(cli_socket, buffer(reply_msg,8), ec);
		    if(!ec){
		    	// accept from server, NEED to do Synchronous accept
		    	boost::system::error_code ec2;
		    	// block here, it's NECESSARY
		    	_acceptor.accept(serv_socket, ec2);
				if(!ec2){
					_acceptor.close();
					// after accept from server, send second reply
					boost::system::error_code ec3;
	    			write(cli_socket, buffer(reply_msg,8), ec3);
	    			if(!ec3){
	    				// data transmission
	    				// asynchronous read from each other
	    				cli_to_serv();
	    				serv_to_cli();
	    			}
	    			else{
	    				reply_state = false;
	    				cerr << "ERROR: second reply failed\n";
	    			}
				}
				else{
					_acceptor.close();
					reply_state = false;
					boost::system::error_code ec3;
					reply_msg[1] = 91;
	    			write(cli_socket, buffer(reply_msg,8), ec3);
					cerr << "ERROR: cannot accept from FTP Server\n";
				}
		    }
		    else{
		    	reply_state = false;
		    	cerr << "ERROR: first reply failed\n";
		    }
		    if(reply_state == true){
		    	cout << "<Reply>: Accept"  << endl << endl;
		    }
		    else{
		    	cout << "<Reply>: Reject"  << endl << endl;
		    	serv_socket.close();
		    	cli_socket.close();
		    	exit(0);
		    }
		}
	}
	void cli_to_serv(){
		auto self(shared_from_this());
		memset(cli_data,'\0',4096);
		// read from client
		cli_socket.async_read_some(
			buffer(cli_data, 4096),
			[this,self](boost::system::error_code ec, size_t length){
				if(!ec){
					boost::system::error_code ec2;
					// write to server
					write(serv_socket, buffer(cli_data, length), ec2);
					if(!ec2){
						cli_to_serv();
					}
					else{
						// do not need to write to server
						// end this child connection
						cli_socket.close();
						serv_socket.close();
						exit(0);
					}
				}
				else{
					// do not need to read from client
					// end this child connection
					cli_socket.close();
					serv_socket.close();
					exit(0);
				}
			});
	}
	void serv_to_cli(){
		auto self(shared_from_this());
		memset(serv_data,'\0',4096);
		// read from server
		serv_socket.async_read_some(
				buffer(serv_data, 4096),
				[this, self](boost::system::error_code ec, size_t length){
					if(!ec){
						boost::system::error_code ec2;
						// write to client
						write(cli_socket, buffer(serv_data, length), ec2);
						if(!ec2){
							serv_to_cli();
						}
						else{
							// do not need to write to client
							// end this child connection
							cli_socket.close();
							serv_socket.close();
							exit(0);
						}
					}
					else{
						// do not need to read from server
						// end this child connection
						cli_socket.close();
						serv_socket.close();
						exit(0);
					}
				});
	}
};
class SocksServer {
private:
	ip::tcp::acceptor acceptor;
	ip::tcp::socket socket;
public:
	// constructor
	SocksServer(short port):
		acceptor(io_serv, ip::tcp::endpoint(ip::tcp::v4(), port)),
		socket(io_serv){
			do_accept();
	}
private:
	void do_accept() {
		acceptor.async_accept(socket, [this](boost::system::error_code ec){
			if(!ec){
				pid_t pid;
				// inform the io_service that the process is about to fork
				//cout << "accept connection from client\n";
				pid = fork();
				if(pid == 0){
					io_serv.notify_fork(io_service::fork_child);
					// parent use acceptor, child doesn't
					acceptor.close();
					// start a session
					make_shared<SocksSession>(move(socket))->start();
				}
				else if(pid > 0){
					io_serv.notify_fork(io_service::fork_parent);
					// child use socket, parent doesn't
					socket.close();
					do_accept();
				}
				else{
					// fork error
					cerr << "Fork error" << endl;
					socket.close();
					do_accept();
				}
			}
		});
	}
};

int main(int argc, char** argv){
	//cout << "server start\n";
	SocksServer SS(atoi(argv[1]));
	io_serv.run();
	return 0;
}