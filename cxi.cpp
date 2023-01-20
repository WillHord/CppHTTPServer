// $Id: cxi.cpp,v 1.6 2021-11-08 00:01:44-08 - - $

#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <fstream>
#include <sstream>
#include <iostream>
using namespace std;

#include <libgen.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "debug.h"
#include "logstream.h"
#include "protocol.h"
#include "socket.h"

logstream outlog (cout);
struct cxi_exit: public exception {};

unordered_map<string,cxi_command> command_map {
   {"exit", cxi_command::EXIT},
   {"help", cxi_command::HELP},
   {"ls"  , cxi_command::LS  },
   {"rm", cxi_command::RM},
   {"put", cxi_command::PUT},
   {"get", cxi_command::GET}
};

static const char help[] = R"||(
exit         - Exit the program.  Equivalent to EOF.
get filename - Copy remote file to local host.
help         - Print help summary.
ls           - List names of files on remote server.
put filename - Copy local file to remote host.
rm filename  - Remove file from remote server.
)||";

bool checkFilename(string filename){
   if(filename.size() > 58 or filename.size() < 1){
      return false;
   } 
   return true;
}

void cxi_help() {
   cout << help;
}

void cxi_ls (client_socket& server) {
   cxi_header header;
   header.command = cxi_command::LS;
   DEBUGF ('h', "sending header " << header << endl);
   send_packet (server, &header, sizeof header);
   recv_packet (server, &header, sizeof header);
   DEBUGF ('h', "received header " << header << endl);
   if (header.command != cxi_command::LSOUT) {
      outlog << "sent LS, server did not return LSOUT" << endl;
      outlog << "server returned " << header << endl;
   }else {
      size_t host_nbytes = ntohl (header.nbytes);
      auto buffer = make_unique<char[]> (host_nbytes + 1);
      recv_packet (server, buffer.get(), host_nbytes);
      DEBUGF ('h', "received " << host_nbytes << " bytes");
      buffer[host_nbytes] = '\0';
      cout << buffer.get();
   }
}

void cxi_rm(client_socket& server, string argument){
   cxi_header header;
   header.command = cxi_command::RM;
   header.nbytes = htonl(0);
   string filename = argument.substr(0, argument.find(' '));
   if(!checkFilename(filename)){
      cerr << "ERROR: Filename: " << filename << " is invalid" << endl;
      return;
   }
   size_t i = 0;
   for(; i < filename.size(); i++){
      header.filename[i] = filename[i];
   }
   header.filename[i] = '\0';

   DEBUGF ('h', "sending header " << header << endl);
   send_packet (server, &header, sizeof header);
   recv_packet (server, &header, sizeof header);
   DEBUGF ('h', "received header " << header << endl);
   if(header.command != cxi_command::ACK){
      cerr << "rm " << filename << ": " 
         << strerror(ntohl(header.nbytes)) << endl;
   } else {
      cout << "OK" << endl;
   }
}

void cxi_put(client_socket& server, string filename){
   if(!checkFilename(filename)){
      cerr << "ERROR: Filename: " << filename << " is invalid" << endl;
      return;
   }
   cxi_header header;
   struct stat stat_buf;
   int status = stat(&filename[0], &stat_buf);
   if(status != 0){
       cerr << "put: cannot put '" << filename
         << "':  No such file or directory" << endl;
       return;
   }

   header.nbytes = htonl(stat_buf.st_size);
   header.command = cxi_command::PUT;
   size_t i = 0;
   for(; i < filename.size(); i++){
      header.filename[i] = filename[i];
   }
   header.filename[i] = '\0';

   auto buffer = make_unique<char[]> (stat_buf.st_size);
   ifstream infile;
   infile.open(filename);
   if(!infile.is_open()){
      cerr << "put: cannot put '" << filename <<
         "':  No such file or directory" << endl;
      return;
   }
   infile.read(buffer.get(), stat_buf.st_size);
   infile.close();
   
   send_packet(server, &header, sizeof header);
   send_packet(server, buffer.get(), stat_buf.st_size);
   
   recv_packet (server, &header, sizeof header);
   if(header.command != cxi_command::ACK){
      cerr << "put " << filename << ": " 
         << strerror(ntohl(header.nbytes)) << endl;
   } else {
      cout << "OK" << endl;
   }
}

void cxi_get(client_socket& server, string filename){
   if(!checkFilename(filename)){
      cerr << "get: filename '" << filename << "' is invalid" << endl;
      return;
   }
   cxi_header header;
   header.nbytes = htonl(0);
   header.command = cxi_command::GET;
   size_t i = 0;
   for(; i < filename.size(); i++){
      header.filename[i] = filename[i];
   }
   header.filename[i] = '\0';

   send_packet(server, &header, sizeof header);
   recv_packet (server, &header, sizeof header);
   if(header.command != cxi_command::FILEOUT){
      cerr << "get " << filename << ": " 
         << strerror(ntohl(header.nbytes)) << endl;
   } else {
      auto buffer = make_unique<char[]> (ntohl(header.nbytes));
      recv_packet(server, buffer.get(), ntohl(header.nbytes));

      ofstream outfile;
      outfile.open(header.filename);
      outfile.write(buffer.get(), ntohl(header.nbytes));
      outfile.close();
      cout << "OK" << endl;
   }
}

void usage() {
   cerr << "Usage: " << outlog.execname() << " host port" << endl;
   throw cxi_exit();
}

pair<string,in_port_t> scan_options (int argc, char** argv) {
   for (;;) {
      int opt = getopt (argc, argv, "@:");
      if (opt == EOF) break;
      switch (opt) {
         case '@': debugflags::setflags (optarg);
                   break;
      }
   }
   if (argc - optind != 2) usage();
   string host = argv[optind];
   in_port_t port = get_cxi_server_port (argv[optind + 1]);
   return {host, port};
}

int main (int argc, char** argv) {
   outlog.execname (basename (argv[0]));
   outlog << to_string (hostinfo()) << endl;
   try {
      auto host_port = scan_options (argc, argv);
      string host = host_port.first;
      in_port_t port = host_port.second;
      outlog << "connecting to " << host << " port " << port << endl;
      client_socket server (host, port);
      outlog << "connected to " << to_string (server) << endl;
      for (;;) {
         string line, temp;
         getline (cin, line);
         if (cin.eof()) throw cxi_exit();
         stringstream s(line);
         vector<string> args;
         while(getline(s, temp, ' ')) args.push_back(temp);

         auto it = args.begin();
         while(it != args.end()){
            
            const auto& itor = command_map.find (*it);
            cxi_command cmd = itor == command_map.end()
                           ? cxi_command::ERROR : itor->second;
            switch (cmd) {
               case cxi_command::EXIT:
                  throw cxi_exit();
                  it += 1;
                  break;
               case cxi_command::HELP:
                  cxi_help();
                  it += 1;
                  break;
               case cxi_command::LS:
                  cxi_ls (server);
                  it += 1;
                  break;
               case cxi_command::RM:
                  if(next(it) == args.end()){
                     cerr << "ERROR: rm takes one filename argument"
                        << endl;
                     it++;
                     break;
                  }
                  cxi_rm(server, *next(it));
                  it += 2;
                  break;
               case cxi_command::PUT:
                  if(next(it) == args.end()){
                     cerr << "ERROR: put takes one filename argument"
                        << endl;
                     it++;
                     break;
                  }
                  cxi_put(server, *next(it));
                  it += 2;
                  break;
               case cxi_command::GET:
                  if(next(it) == args.end()){
                     cerr << "ERROR: get takes one filename argument"
                        << endl;
                     it++;
                     break;
                  }
                  cxi_get(server, *next(it));
                  it += 2;
                  break;
               default:
                  outlog << line << ": invalid command" << endl;
                  it++;
                  break;
            }
         }
      }
   }catch (socket_error& error) {
      outlog << error.what() << endl;
   }catch (cxi_exit& error) {
      DEBUGF ('x', "caught cxi_exit");
   }
   return 0;
}

