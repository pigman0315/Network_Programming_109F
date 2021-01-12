#include <cstdlib>
#include <iostream>
#include <memory>
#include <stdio.h>
#include <fcntl.h>
#include <string>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <utility>
#include <boost/asio.hpp>
#include <boost/algorithm/string.hpp>
#include <vector>
#include <stdlib.h>
using boost::asio::ip::tcp;
using namespace std;
using namespace boost;
class session
  : public std::enable_shared_from_this<session>
{
public:
  session(tcp::socket socket)
    : socket_(std::move(socket))
  {
  }

  void start()
  {
    do_read();
  }

private:
  void parse_header(char* data)
  {
    string req_mtd, req_url, qry_str, serv_prtc,host, serv_addr, serv_port, remote_addr, remote_port;
    string header(data);
    if(header.length() == 0){
      cout << "Empty" << endl;
      return;
    }
    vector<string> split_vec;
    split(split_vec, header, is_any_of("\r\n"), token_compress_on);
    // set request method
    vector<string> s0;
    split(s0,split_vec[0],is_any_of(" "), token_compress_on);
    req_mtd = s0[0];
    setenv("REQUEST_METHOD",req_mtd.c_str(),1);
    // set request url
    vector<string> s1;
    split(s1,split_vec[1],is_any_of(" "), token_compress_on);
    req_url = s0[1];
    setenv("REQUEST_URI",req_url.c_str(),1);
    // set qry_str
    vector<string> s_question;
    split(s_question,s0[1],is_any_of("?"));
    if(s_question.size() >= 2){
      qry_str = s_question[1];
      setenv("QUERY_STRING",qry_str.c_str(),1);
    }
    // set host name
    host = split_vec[1];
    vector<string> s_host;
    split(s_host,host,is_any_of(" "));
    setenv("HTTP_HOST",s_host[1].c_str(),1);
    // set server protocol
    serv_prtc = s0[2];
    setenv("SERVER_PROTOCOL",serv_prtc.c_str(),1);
    
    // set server address & port
    serv_addr = socket_.local_endpoint().address().to_string();
    serv_port = to_string(socket_.local_endpoint().port());
    setenv("SERVER_ADDR",serv_addr.c_str(),1);
    setenv("SERVER_PORT",serv_port.c_str(),1);
    // debug
    
    // set remote address & port
    remote_addr = socket_.remote_endpoint().address().to_string();
    remote_port = to_string(socket_.remote_endpoint().port());
    setenv("REMOTE_ADDR",remote_addr.c_str(),1);
    setenv("REMOTE_PORT",remote_port.c_str(),1);
    
  
    // set cgi program name
    string cgi_name = erase_head_copy(s_question[0],1);
    setenv("CGI_NAME",cgi_name.c_str(),1);
    //cout << req_mtd << " " << req_url << " " << qry_str <<  " " << serv_addr << " " << serv_prtc << " " << serv_port << " " << remote_addr<<" " << remote_port << " " << cgi_name <<endl;
  }
  void do_read()
  {
    auto self(shared_from_this());
    socket_.async_read_some(boost::asio::buffer(data_, max_length),
        [this, self](boost::system::error_code ec, std::size_t leng)
        {
          if (!ec)
          {
            pid_t pid;
            pid = fork();
            if(pid < 0){
              cerr << "Fork error\n";
            }
            else if(pid == 0){
              // set environment variable
              parse_header(data_);
              int fd = socket_.native_handle();
              // redirect fd to stdin
              dup2(fd,STDIN_FILENO);
              // redirect fd to stdout 
              dup2(fd,STDOUT_FILENO);
              // redirect fd to stderr 
              dup2(STDOUT_FILENO,STDERR_FILENO);
              //
              // execute cgi program
              vector<string> tmp_v;
              string cgi_name = getenv("CGI_NAME");
              //cerr << cgi_name << endl;
              //cerr << cgi_name.length() << endl;
              
              string path = "./" + cgi_name;
              cout << "HTTP/1.1 200 OK\r\n";
              execl(path.c_str(),cgi_name.c_str(),NULL);
              cerr << "ERROR: exec error" << endl;
              exit(0);
            }
            socket_.close();
          }
        });
  }

  void do_write()
  {
    auto self(shared_from_this());
    std::string data;
    data += "HTTP/1.1 200 OK\r\n";
    data += "Content-type: text/html\r\n";
    data += "\r\n";
    data += "<html>";
    data += "<body>";
    data += "<h2>aaa<h2>";
    data += "</body>";
    data += "</html>";
    data += "\r\n";
    boost::asio::async_write(socket_, boost::asio::buffer(data.c_str(), data.length()),
        [this, self](boost::system::error_code ec, std::size_t /*length*/)
        {
          if (!ec)
          {
            //
          }
        });
  }

  tcp::socket socket_;
  enum { max_length = 1024 };
  char data_[max_length];
};

class server
{
public:
  server(boost::asio::io_context& io_context, short port)
    : acceptor_(io_context, tcp::endpoint(tcp::v4(), port))
  {
    do_accept();
  }

private:
  void do_accept()
  {
    // lambda function: accept handler
    acceptor_.async_accept(
        [this](boost::system::error_code ec, tcp::socket socket)
        {
          if (!ec)
          {
            boost::asio::socket_base::reuse_address option(true);
            acceptor_.set_option(option);
            std::make_shared<session>(std::move(socket))->start();
          }
          socket.close();
          do_accept();
        });
  }
  // member
  tcp::acceptor acceptor_;
};

int main(int argc, char* argv[])
{
  printf("server start\n");
  try
  {
    if (argc != 2)
    {
      std::cerr << "Usage: async_tcp_echo_server <port>\n";
      return 1;
    }

    boost::asio::io_context io_context;

    server s(io_context, std::atoi(argv[1]));

    io_context.run();
  }
  catch (std::exception& e)
  {
    std::cerr << "Exception: " << e.what() << "\n";
  }

  return 0;
}