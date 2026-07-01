#include <iostream>
#include <string>
#include <vector>
#include <cstdlib>
#include <sstream>
#include <filesystem>
#include <algorithm>
#include <unistd.h>
#include <sys/wait.h>
#include <system_error>

std::vector<std::string> parseArgs(std::string &line){
  std::vector<std::string> args;
  std::string curr;
  int ix=0,n=line.size();

  while(ix<n){
    char c=line[ix];
    if(c == ' ' && !curr.empty()){
      args.push_back(curr);
      curr="";
      ix++;
      continue;
    }
    if(c == ' '){
      ix++;
      continue;
    }

    if(c == '\\'){
      ix++;
      if(ix<n){
        curr+=line[ix];
        ix++;
      }
      continue;
    }

    if(c == '\''){
      ix++;
      while(ix<n && line[ix]!='\''){
        curr+=line[ix];
        ix++;
      }
      ix++;
      continue;
    }

    if(c == '"'){
      ix++;
      while(ix<n && line[ix]!='"'){
        if(line[ix]=='\\' && ix+1<n){
          char next=line[ix+1];
          if(next=='"' || next=='\\' || next=='$' || next=='`'){
            curr+=next;
            ix+=2;
            continue;
          }
        }
        curr+=line[ix];
        ix++;
      }
      ix++;
      continue;
    }

    curr+=c;
    ix++;
  }

  if(!curr.empty()){
    args.push_back(curr);
    curr="";
  }

  return args;
}

int main() {
  // Flush after every std::cout / std:cerr
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf; 
  std::vector<std::string> builtins = {"echo" , "type" , "exit" , "pwd" , "cd"};

  // REPL
  while(1){
    std::cout<<"$ ";
    std::string line;
    std::getline(std::cin,line);

    // if the line is empty
    if(line.empty()){
      continue;
    }

    // reading the command and the args
    std::vector<std::string> arguments=parseArgs(line);
    if(arguments.empty()){
      continue;
    }
    std::string command = arguments[0];
    std::vector<std::string> args(arguments.begin()+1,arguments.end());


    /*
     * <-----------------------------------COMMMANDS----------------------------------------->
     */

    if(command == "exit"){
      break;
    }
    else if(command == "echo"){
      for(int i=0;i<args.size();i++){
        if(i) std::cout<<" ";
        std::cout<<args[i];
      }
      std::cout<<std::endl;
    }
    else if(command == "pwd"){
      std::cout<<std::filesystem::current_path().string()<<std::endl;
    }
    else if(command == "cd"){
      if(args.size() == 0){
        continue;
      }
      std::string path = args[0];
      if(path == "~"){
        path = getenv("HOME");
      }
      std::error_code ec;
      std::filesystem::current_path(path,ec);
      if(ec) std::cout<<command<<":"<<line.substr(2)<<": No such file or directory"<<std::endl;
    }
    else if(command == "type"){
      if(args.size()==0){
        continue;
      }
      if(std::find(builtins.begin(),builtins.end(),args[0])!=builtins.end()){
        std::cout<<args[0]<<" is a shell builtin"<<std::endl;
      }
      else{
        bool found = false;
        char* env = std::getenv("PATH");
        if(env == nullptr){
            std::cout<<args[0]<<": not found"<<std::endl;
            continue;
        }
        std::string path = env;
        std::stringstream ss(path);
        std::string dir;

        while(std::getline(ss,dir,':')){
          std::string pathStr = dir+"/"+args[0];
          if(access(pathStr.c_str(),X_OK) == 0){
              found = true;
              std::cout<<args[0]<<" is "<<pathStr<<std::endl;
              break;
          }
        }
        if(!found) std::cout<<args[0]<<": not found"<<std::endl;
      }
    }
    else{
      std::vector<const char*> argsc;
      argsc.push_back(command.c_str());
      for(auto &x : args){
        argsc.push_back(x.c_str());
      } 
      argsc.push_back(nullptr);

      pid_t pid = fork();
      if(pid==0){
        execvp(command.c_str() , const_cast<char**>(argsc.data()));
        std::cerr<<line<<": not found"<<std::endl;
        exit(1);
      }
      else if(pid>0){
        int status;
        waitpid(pid,&status,0);
      }
      else{
        std::cerr<<"fork failed"<<std::endl;
      }
    }
  }
}
