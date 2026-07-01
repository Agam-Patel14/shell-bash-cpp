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
#include <fcntl.h>

struct parsedCommand {
  std::string command;
  std::vector<std::string> args;
  bool redirectStdout = false;
  std::string outputFile;
};

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

bool redirectStdout(const std::string &file){
  int fd = open(file.c_str(),O_WRONLY | O_CREAT | O_TRUNC ,0644);

  if(fd == -1){
    perror("open");
    return false;
  }

  if(dup2(fd, STDOUT_FILENO) == -1){
    perror("dup2");
    close(fd);
    return false;
  }
  close(fd);
  return true;
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
    parsedCommand input;
    input.command = arguments[0];
    for(int i=1 ; i<arguments.size() ; i++){
      if(arguments[i] == ">" || arguments[i] == "1>"){
        if(i+1<arguments.size()){
          input.outputFile = arguments[i+1];
          input.redirectStdout = true;
          i++;
        }
      }
      else{
        input.args.push_back(arguments[i]);
      }
    }

    bool isBuitin = std::find(builtins.begin(),builtins.end(),input.command)!=builtins.end();

    /*
     * <-----------------------------------COMMMANDS----------------------------------------->
     */

    int saved = -1;
    if(isBuitin && input.redirectStdout){
      saved = dup(STDOUT_FILENO);
      if(saved == -1) continue;
      if(!redirectStdout(input.outputFile)){
        close(saved);
        continue;
      }
    }

    if(input.command == "exit"){
      break;
    }
    else if(input.command == "echo"){
      for(int i=0;i<input.args.size();i++){
        if(i) std::cout<<" ";
        std::cout<<input.args[i];
      }
      std::cout<<std::endl;
    }
    else if(input.command == "pwd"){
      std::cout<<std::filesystem::current_path().string()<<std::endl;
    }
    else if(input.command == "cd"){
      if(input.args.size() == 0){
        // continue;
      }
      else if(input.args.size()>2){
        std::cout<<"cd: too many arguments"<<std::endl;
      }
      else{
        std::string path = input.args[0];
        if(path == "~"){
          path = getenv("HOME");
        }
        std::error_code ec;
        std::filesystem::current_path(path,ec);
        if(ec) std::cout<<input.command<<":"<<line.substr(2)<<": No such file or directory"<<std::endl;
      }
    }
    else if(input.command == "type"){
      if(input.args.size()!=0){
        int numArgs = input.args.size();
        for(int i=0 ; i<numArgs ; i++){
          if(std::find(builtins.begin(),builtins.end(),input.args[i])!=builtins.end()){
            std::cout<<input.args[i]<<" is a shell builtin"<<std::endl;
          }
          else{
            bool found = false;
            char* env = std::getenv("PATH");
            if(env == nullptr){
                std::cout<<input.args[i]<<": not found"<<std::endl;
                continue;
            }
            std::string path = env;
            std::stringstream ss(path);
            std::string dir;

            while(std::getline(ss,dir,':')){
              std::string pathStr = dir+"/"+input.args[i];
              if(access(pathStr.c_str(),X_OK) == 0){
                  found = true;
                  std::cout<<input.args[i]<<" is "<<pathStr<<std::endl;
                  break;
              }
            }
            if(!found) std::cout<<input.args[i]<<": not found"<<std::endl;
          }
        }
      }  
    }
    else{
      std::vector<const char*> argsc;
      argsc.push_back(input.command.c_str());
      for(auto &x : input.args){
        argsc.push_back(x.c_str());
      } 
      argsc.push_back(nullptr);

      pid_t pid = fork();
      if(pid==0){
        if(input.redirectStdout){
          if(!redirectStdout(input.outputFile)){
            exit(1);
          }
        }
        execvp(input.command.c_str() , const_cast<char**>(argsc.data()));
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

    if(isBuitin && input.redirectStdout){
      if(dup2(saved, STDOUT_FILENO) == -1){
        perror("dup2");
      }
      close(saved);
    }
  }
}
