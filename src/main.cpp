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
#include <readline/readline.h>
#include <readline/history.h>

struct parsedCommand {
  std::string command;
  std::vector<std::string> args;
  bool redirectStdout = false;
  bool redirectStderr = false;
  bool appendStdout = false;
  bool appendStderr = false;
  std::string outputFile;
  std::string errorFile;
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

bool redirectFd(int fileno , const std::string &file , bool append){
  int flag =  O_WRONLY | O_CREAT;
  if(append) flag |= O_APPEND;
  else flag |= O_TRUNC;
  int fd = open(file.c_str(),flag,0644);

  if(fd == -1){
    perror("open");
    return false;
  }
  if(dup2(fd,fileno) == -1){
    perror("dup2");
    close(fd);
    return false;
  }
  close(fd);
  return true;
}

std::vector<std::string> getPathExecutables(){
  std::vector<std::string> executables;
  char* env = std::getenv("PATH");
  if(env == nullptr) return executables;
  
  std::string path = env;
  std::stringstream ss(path);
  std::string dir;
  
  while(std::getline(ss,dir,':')){
    if(dir.empty()) continue;
    try{
      for(const auto& entry : std::filesystem::directory_iterator(dir)){
        std::string pathStr = entry.path().string();
        if(access(pathStr.c_str(),X_OK) == 0) {
          executables.push_back(entry.path().filename().string());
        }
      }
    } 
    catch(...){
      continue;
    }
  }

  return executables;
}

std::vector<std::string> getAllCommands(){
  std::vector<std::string> commands = {"echo" , "exit" , "type" , "pwd" , "cd" , "complete"};
  std::vector<std::string> path_executables = getPathExecutables();
  commands.insert(commands.end(),path_executables.begin(),path_executables.end());
  
  std::sort(commands.begin(),commands.end());
  commands.erase(std::unique(commands.begin(),commands.end()),commands.end());  
  return commands;
}

char* commandGenerator(const char* text , int state){
  static int ix;
  static std::vector<std::string> commands;
  
  if(!state){
    ix=0;
    commands = getAllCommands();
  }
  
  while(ix < commands.size()){
    const char* cmd = commands[ix].c_str();
    ix++;
    if(strncmp(cmd,text,strlen(text)) == 0){
      return strdup(cmd);
    }
  }
  return nullptr;
}

char** commandCompletion(const char* text , int start , int end){
  if(start != 0){
    return nullptr;
  }
  return rl_completion_matches(text,commandGenerator);
}

int main() {
  // Flush after every std::cout / std:cerr
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf; 
  std::vector<std::string> builtins = {"echo" , "type" , "exit" , "pwd" , "cd" , "complete"};

  rl_attempted_completion_function = commandCompletion;
  // REPL
  while(1){
    char* userInput = readline("$ ");

    if(!userInput) break;
    std::string line(userInput);
    if(!line.empty()){
      add_history(userInput);
    }
    free(userInput);

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
      else if(arguments[i] == "2>"){
        if(i+1<arguments.size()){
          input.errorFile = arguments[i+1];
          input.redirectStderr = true;
          i++;
        }
      }
      else if(arguments[i] == ">>" || arguments[i] == "1>>"){
        if(i+1<arguments.size()){
          input.outputFile = arguments[i+1];
          input.appendStdout = true;
          input.redirectStdout = true;
          i++;
        }
      }
      else if(arguments[i] == "2>>"){
        if(i+1<arguments.size()){
          input.errorFile = arguments[i+1];
          input.appendStderr = true;
          input.redirectStderr = true;
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

    int savedout = -1 , savederr = -1;
    if(isBuitin && input.redirectStdout){
      savedout = dup(STDOUT_FILENO);
      if(savedout == -1) continue;
      if(!redirectFd(STDOUT_FILENO,input.outputFile,input.appendStdout)){
        close(savedout);
        continue;
      }
    }
    if(isBuitin && input.redirectStderr){
      savederr = dup(STDERR_FILENO);
      if(savederr == -1) continue;
      if(!redirectFd(STDERR_FILENO,input.errorFile,input.appendStderr)){
        close(savederr);
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
    else if(input.command == "complete"){
      if(input.args[0] == "-p"){
        std::cout<<"complete: "<<input.args[1]<<": no completion specification"<<std::endl;
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
          if(!redirectFd(STDOUT_FILENO,input.outputFile,input.appendStdout)){
            exit(1);
          }
        }
        if(input.redirectStderr){
          if(!redirectFd(STDERR_FILENO,input.errorFile,input.appendStderr)){
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
      if(dup2(savedout,STDOUT_FILENO) == -1){
        perror("dup2");
      }
      close(savedout);
    }
    if(isBuitin && input.redirectStderr){
      if(dup2(savederr,STDERR_FILENO) == -1){
        perror("dup2");
      }
      close(savederr);
    }
  }
}
