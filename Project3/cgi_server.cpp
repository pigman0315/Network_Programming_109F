#include <cstdlib>
#include <iostream>
#include <memory>
#include <fstream>
#include <stdio.h>
#include <fcntl.h>
#include <string>
#include <unistd.h>
#include <utility>
#include <boost/asio.hpp>
#include <boost/algorithm/string.hpp>
#include <io.h>
#include <vector>
#include <stdlib.h>
#include <boost/bind.hpp>
#include <Windows.h>
#define MAX_LENGTH 4096

using namespace std;
using namespace boost::asio;
//
bool is_session[5] = {false, false, false, false, false};
io_service io_serv;
string Host[5],Port[5],FileName[5];

void parse_query(string str){
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

class HttpSession : public std::enable_shared_from_this<HttpSession>
{
private:
  ip::tcp::socket socket_;
  enum { max_length = 1024 };
  char data_[max_length];
public:
  HttpSession(ip::tcp::socket socket)
    : socket_(std::move(socket)){}
  void start()
  {
    do_read();
  }
private:
  void parse(char* data)
  {
    string req_mtd, req_url, qry_str, serv_prtc,host, serv_addr, serv_port, remote_addr, remote_port;
    string header(data);
    if(header.length() == 0){
      cout << "Empty" << endl;
      return;
    }
    vector<string> split_vec;
    boost::split(split_vec, header, boost::is_any_of("\r\n"), boost::token_compress_on);
    // set request method
    vector<string> s0;
    boost::split(s0,split_vec[0],boost::is_any_of(" "), boost::token_compress_on);
    req_mtd = s0[0];
    //setenv("REQUEST_METHOD",req_mtd.c_str(),1);
    // set request url
    vector<string> s1;
    boost::split(s1,split_vec[1],boost::is_any_of(" "), boost::token_compress_on);
    req_url = "http://"+s1[1]+s0[1];
    //setenv("REQUEST_URL",req_url.c_str(),1);
    // set qry_str
    vector<string> s_question;
    boost::split(s_question,s0[1],boost::is_any_of("?"));
    if(s_question.size() >= 2){
      qry_str = s_question[1];
      _putenv_s("QUERY_STRING",qry_str.c_str());
    }
    // set server protocol
    serv_prtc = s0[2];
    //setenv("SERVER_PROTOCOL",serv_prtc.c_str(),1);
    
    // set server address & port
    serv_addr = socket_.local_endpoint().address().to_string();
    serv_port = to_string(socket_.local_endpoint().port());
    //setenv("SERVER_ADDR",serv_addr.c_str(),1);
    //setenv("SERVER_PORT",serv_port.c_str(),1);
    // debug
    
    // set remote address & port
    remote_addr = socket_.remote_endpoint().address().to_string();
    remote_port = to_string(socket_.remote_endpoint().port());
    //setenv("REMOTE_ADDR",remote_addr.c_str(),1);
    //setenv("REMOTE_PORT",remote_port.c_str(),1);
    
  
    // set cgi program name
    string cgi_name = boost::algorithm::erase_head_copy(s_question[0],1);
    _putenv_s("CGI_NAME",cgi_name.c_str());
    cout << req_mtd << " " << req_url << " " << qry_str <<  " " << serv_addr << " " << serv_prtc << " " << serv_port << " " << remote_addr<<" " << remote_port << " " << cgi_name <<endl;
 } 
  void do_read()
  {
    auto self(shared_from_this());
    socket_.async_read_some(buffer(data_, max_length),
        [this, self](boost::system::error_code ec, std::size_t leng)
        {
          if(!ec){
              cout << data_ << endl;
              parse(data_);
              string cgi_name = getenv("CGI_NAME");
              send_html_content(cgi_name);   
          }
        });
  }
  void send_html_content(string cgi_name){
    auto self(shared_from_this());
    if(cgi_name == "panel.cgi"){
      string content;
      content += "HTTP/1.1 200 OK\r\n";
      content += "Content-type: text/html\r\n\r\n";
      content += "<!DOCTYPE html>";
      content += "<head><title>NP Project 3 Panel</title><linkrel=\"stylesheet\"href=\"https://cdn.jsdelivr.net/npm/bootstrap@4.5.3/dist/css/bootstrap.min.css\"integrity=\"sha384-TX8t27EcRE3e/ihU7zmQxVncDAy5uIKz4rEkgIXeMed4M0jlfIDPvg6uqKI2xXr2\"crossorigin=\"anonymous\"/><linkhref=\"https://fonts.googleapis.com/css?family=Source+Code+Pro\"rel=\"stylesheet\"/><linkrel=\"icon\"type=\"image/png\"href=\"https://cdn4.iconfinder.com/data/icons/iconsimple-setting-time/512/dashboard-512.png\"/><style>* {font-family: \'Source Code Pro\', monospace;}</style></head>";
      content += "<body class=\"bg-secondary pt-5\">";
      content = content + "<form action=\"console.cgi\" method=\"GET\"><table class=\"table mx-auto bg-light\" style=\"width: inherit\"><thead class=\"thead-dark\"><tr><th scope=\"col\">#</th><th scope=\"col\">Host</th><th scope=\"col\">Port</th><th scope=\"col\">Input File</th></tr></thead><tbody>";
      // 5 session
      for(int i = 0;i < 5;i++){
        content += "<tr>";
        content = content + "<th scope=\"row\" class=\"align-middle\">Session "+to_string(i+1)+"</th>";
        content += "<td><div class=\"input-group\"><select name=\"h"+to_string(i)+"\" class=\"custom-select\">";
        // 12 nplinux server
        for(int j = 1;j <= 12;j++){
          content = content + "<option></option><option value=\"nplinux"+to_string(j)+".cs.nctu.edu.tw\">nplinux"+to_string(j)+"</option>";
        }
        content = content + "</select><div class=\"input-group-append\"><span class=\"input-group-text\">.cs.nctu.edu.tw</span></div></div></td>";
        content = content + "<td><input name=\"p"+to_string(i)+"\" type=\"text\" class=\"form-control\" size=\"5\" /></td>";
        content = content + "<td><select name=\"f"+to_string(i)+"\" class=\"custom-select\">";
        // 10 files
        for(int j = 1;j <= 10;j++){
          content = content + "<option></option><option value=\"t"+to_string(j)+".txt\">t"+to_string(j)+".txt</option>";
        }
        content += "</select></td>";
      }
      content += "<tr><td colspan=\"3\"></td><td><button type=\"submit\" class=\"btn btn-info btn-block\">Run</button></td></tr></tbody></table></form></body></html>";
      boost::asio::async_write(socket_, boost::asio::buffer(content.c_str(), content.length()),
        [this, self](boost::system::error_code ec, std::size_t /*length*/)
        {
          if (!ec){
            cout << "send ok" << endl;
          }
      });
    }
    else if(cgi_name == "console.cgi"){
      string query = getenv("QUERY_STRING");
      parse_query(query);
      // initial is_session
      for(int i = 0;i < 5;i++){
        is_session[i] = false;
      }
      for(int i = 0;i < 5;i++){
        if(Host[i] != "None")
          is_session[i] = true;
      }
      string content;
      content += "HTTP/1.1 200 OK\r\n";
      content += "Content-type: text/html\r\n\r\n";
      content += "<!DOCTYPE html><html lang=\"en\"><head><meta charset=\"UTF-8\" /><title>NP Project 3 Console</title>";
      content += "<link rel=\"stylesheet\" href=\"https://stackpath.bootstrapcdn.com/bootstrap/4.1.3/css/bootstrap.min.css\" integrity=\"sha384-MCw98/SFnGE8fJT3GXwEOngsV7Zt27NXFoaoApmYm81iuXoPkFOJwJ8ERdknLPMO\" crossorigin=\"anonymous\"/><link href=\"https://fonts.googleapis.com/css?family=Source+Code+Pro\" \rel=\"stylesheet\" /> <link rel=\"icon\" type=\"image/png\" href=\"https://cdn0.iconfinder.com/data/icons/small-n-flat/24/678068-terminal-512.png\" /> <style> * { font-family: 'Source Code Pro', monospace; font-size: 1rem !important;} body {background-color: #212529;} pre {color: #cccccc;} b {color: #01b468;} </style> </head> <body> <table class=\"table table-dark table-bordered\"> <thead> <tr>";
      for(int i = 0; i < 5; i++) {
        if(is_session[i] == true)
          content = content + "<th scope=\"col\">" + Host[i] + ":" + Port[i] + "</th>";
      }
      content +=  "</tr></thead>";
      content +=  "<tbody><tr>";
      for(int i = 0; i < 5; i++) {
        if(is_session[i] == true)
            content = content + "<td><pre id=\"s" + to_string(i) + "\" class=\"mb-0\"></pre></td>";
      }
      content += "</tr></tbody></table></body></html>";
      boost::asio::async_write(socket_, boost::asio::buffer(content.c_str(), content.length()),
        [this, self](boost::system::error_code ec, std::size_t /*length*/)
        {
          if (!ec){
            cout << "send ok" << endl;
          }
      });
      for(int i = 0; i < 5; i++){
        if(is_session[i]) {
          make_shared<ShellSession>(i, self)->start();      
        }
      }
    }
    else{
      cout << "ERROR send html content" << endl;
    }
  }
  void do_write()
  {
    auto self(shared_from_this());
    string msg;
    msg += "HTTP/1.1 200 OK\r\n";
    msg += "Content-type: text/html\r\n\r\n";
    msg += "\r\n";
    msg += "<html>";
    msg += "<body>";
    msg += "<h2>aaa<h2>";
    msg += "</body>";
    msg += "</html>";
    msg += "\r\n";
    boost::asio::async_write(socket_, boost::asio::buffer(msg.c_str(), msg.length()),
        [this, self](boost::system::error_code ec, std::size_t /*length*/)
        {
          if (!ec){
            cout << "send ok" << endl;
          }
        });
  }
private:
  class ShellSession : public std::enable_shared_from_this<ShellSession>{
    private:
      ip::tcp::socket _socket;
      ip::tcp::resolver _resolver;
      std::array<char, MAX_LENGTH> data;
      shared_ptr<HttpSession> hs;
      ifstream fileFd;
      int num;
      bool flg_exit = false;
    public:
      ShellSession(int n, shared_ptr<HttpSession> &h)
        : _socket(io_serv),
          _resolver(io_serv),
          num(n),
          hs(h)
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
        p = Port[num];
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
              boost::algorithm::replace_all(str, "\"", "&quot;");
              boost::algorithm::replace_all(str, "\r", "");
              string html_str = string("<script>document.getElementById(\"") + string("s") + to_string(num)+"\").innerHTML += \"" + str + "\";</script>";
              boost::asio::async_write(hs->socket_, boost::asio::buffer(html_str.c_str(), html_str.length()),
              [this, self](boost::system::error_code ec, std::size_t /*length*/)
              {
                if (!ec){
                  cout << "send ok" << endl;
                }
              });
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
        boost::asio::async_write(hs->socket_, boost::asio::buffer(html_cmd.c_str(), html_cmd.length()),
        [this, self](boost::system::error_code ec, std::size_t /*length*/)
        {
          if (!ec){
            cout << "send ok" << endl;
          }
        });
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
};

class HttpServer
{
private:
  ip::tcp::acceptor acceptor_;
public:
 HttpServer(short port)
    : acceptor_(io_serv, ip::tcp::endpoint(ip::tcp::v4(), port)){
    do_accept();
  }

private:
  void do_accept()
  {
    acceptor_.async_accept(
        [this](boost::system::error_code ec, ip::tcp::socket socket)
        {
          if (!ec)
          {
            socket_base::reuse_address option(true);
            acceptor_.set_option(option);
            make_shared<HttpSession>(move(socket))->start();
          }
          do_accept();
        });
  }
};

int main(int argc, char* argv[])
{
  printf("server start\n");
  if(argc != 2){
    cout << "./cgi_server [port]" << endl;
  }
  // http server start
  HttpServer HS(std::atoi(argv[1]));
  // select start
  io_serv.run();
  return 0;
}