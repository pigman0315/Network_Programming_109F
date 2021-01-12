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
#include <boost/bind.hpp>
#include <boost/asio.hpp>
#include <boost/algorithm/string.hpp>
#include <time.h>
#define MAX_LENGTH 4096
using namespace std;
using namespace boost::asio;
io_service io_serv;
string Host[5],Port[5],FileName[5];
//
void parse(string str){
	for(int i = 0;i < 5;i++){
		Host[i] = "None";
		Port[i] = "None";
		FileName[i] = "None";
	}
	vector<string> split_str;
	vector<string> tmp;
	boost::split(split_str,str,boost::is_any_of("&"));
	for(int i = 0;i < split_str.size();i+=3){
		boost::split(tmp,split_str[i],boost::is_any_of("="));
		if(tmp[1] != ""){
			Host[i/3] = tmp[1];
		}
	}
	
	for(int i = 1;i < split_str.size();i+=3){
		boost::split(tmp,split_str[i],boost::is_any_of("="));
		if(tmp[1] != ""){
			Port[(i-1)/3] = tmp[1];
		}
	}
	
	for(int i = 2;i<split_str.size();i+=3){
		boost::split(tmp,split_str[i],boost::is_any_of("="));
		if(tmp[1] != ""){
			FileName[(i-2)/3] = tmp[1];
		}
	}
	
}
class ShellSession : public enable_shared_from_this<ShellSession> {
private:
	ip::tcp::socket _socket;
	ip::tcp::resolver _resolver;
	std::array<char, MAX_LENGTH> data;
	ifstream fileFd;
	int num;
	bool flg_exit = false;
public:
	ShellSession(int n)
    : _socket(io_serv),
      _resolver(io_serv),
      num(n)
	  {
	  	string file_path = "./test_case/" + FileName[num];
	  	fileFd.open(file_path);
	  }
	void start() { do_connect(); }
private:
	void do_connect(){
		auto self(shared_from_this());
		string h, p;
		h = Host[num];
		p =	Port[num];
		ip::tcp::resolver::query query(h,p);
		_resolver.async_resolve(query,
				boost::bind(&ShellSession::resolve_handler, self, 
				boost::asio::placeholders::error,
				boost::asio::placeholders::iterator));
	}
	void resolve_handler(const boost::system::error_code& ec,
		ip::tcp::resolver::iterator endpoint_iterator){
		auto self(shared_from_this());
		if(!ec){
			ip::tcp::endpoint endpoint = *endpoint_iterator;
			_socket.async_connect(endpoint,
				boost::bind(&ShellSession::connect_handler, self,
        		boost::asio::placeholders::error, ++endpoint_iterator));
		}
		else{
			cout << "ERROR: resolver handler" << endl;
		}
	}
	void connect_handler(const boost::system::error_code& ec,
		ip::tcp::resolver::iterator endpoint_iterator) {
		auto self(shared_from_this());
		if(!ec){
			do_read();
		}
		else{
			cout << "ERROR: connect_handler" << endl;
		}
	}
	void do_read(){
		auto self(shared_from_this());
		data.fill('\0');
		_socket.async_read_some(buffer(data, MAX_LENGTH),
			[this, self](boost::system::error_code ec, size_t length) {
				if(!ec){
					string str = data.data();
					boost::algorithm::replace_all(str, "\n", "&NewLine;");
					boost::algorithm::replace_all(str, "\r", "");
					boost::algorithm::replace_all(str, "\"", "&quot;");
					string html_str = string("<script>document.getElementById(\"") + string("s") + to_string(num)+"\").innerHTML += \"" + str + "\";</script>";
					cout << html_str << endl;
					if(flg_exit == true){
						_socket.close();
						fileFd.close();
						return;
					}

					if(str.find("% ") != string::npos){
						do_write();
					}
					else{
						do_read();
					}
				}
			});
	}
	void do_write(){
		auto self(shared_from_this());
		string cmd, html_cmd;// cmd for np_golden, html_cmd for html
		if(!getline(fileFd, cmd))
			return;
		cmd += "\n";
		html_cmd = cmd;
		boost::algorithm::replace_all(html_cmd, "\n", "&NewLine;");
		boost::algorithm::replace_all(html_cmd, "\r", "");
		boost::algorithm::replace_all(html_cmd, "\"", "&quot;");
		html_cmd = string("<script>document.getElementById(\"") + string("s") + to_string(num) + "\").innerHTML += \"<b>" + html_cmd + "</b>\";</script>";
		cout << html_cmd << endl;
		if(cmd == "exit\n"){
			flg_exit = true;
		}
		_socket.async_send(
			buffer(cmd),
			[this, self](boost::system::error_code ec, size_t /* length */) {
			if (!ec) { 
				do_read();
			}
        });
	}
};
int main(){
	
	string qry_str = getenv("QUERY_STRING");
	parse(qry_str);
	bool is_session[5] = {false, false, false, false, false};
	for(int i = 0;i < 5;i++){
		if(Host[i] != "None")
			is_session[i] = true;
		//cerr << Host[i] << " " << Port[i] << " " << FileName[i] << endl;
	}
	// send response to client
	string data;
    cout << "HTTP/1.1 200 OK\r\n";
    cout << "Content-type: text/html\r\n\r\n";
    cout << "<!DOCTYPE html><html lang=\"en\"><head><meta charset=\"UTF-8\" /><title>NP Project 3 Console</title>";
   	cout << "<link rel=\"stylesheet\" href=\"https://stackpath.bootstrapcdn.com/bootstrap/4.1.3/css/bootstrap.min.css\" integrity=\"sha384-MCw98/SFnGE8fJT3GXwEOngsV7Zt27NXFoaoApmYm81iuXoPkFOJwJ8ERdknLPMO\" crossorigin=\"anonymous\"/><link href=\"https://fonts.googleapis.com/css?family=Source+Code+Pro\" \rel=\"stylesheet\" /> <link rel=\"icon\" type=\"image/png\" href=\"https://cdn0.iconfinder.com/data/icons/small-n-flat/24/678068-terminal-512.png\" /> <style> * { font-family: 'Source Code Pro', monospace; font-size: 1rem !important;} body {background-color: #212529;} pre {color: #cccccc;} b {color: #01b468;;} </style> </head> <body> <table class=\"table table-dark table-bordered\"> <thead> <tr>";
   	for(int i = 0; i < 5; i++) {
   		if(is_session[i] == true)
			cout << "<th scope=\"col\">" + Host[i] + ":" + Port[i] + "</th>";
	}
    cout<< "</tr></thead>";
    cout << "<tbody><tr>";
	for(int i = 0; i < 5; i++) {
		if(is_session[i] == true)
    		cout << "<td><pre id=\"s" + to_string(i) + "\" class=\"mb-0\"></pre></td>";
	}
	cout << "</tr></tbody></table></body></html>" << endl;
    //
	for(int i = 0; i < 5; i++){
		if(is_session[i]) {
			make_shared<ShellSession>(i)->start();			
		}
	}
	io_serv.run();
}