#include <iostream>
#include <string>
#include <vector>
#include <cstdlib>
#include <sstream>
#include <filesystem>
#include <unistd.h>
#include <algorithm>

int main() {
  // Flush after every std::cout / std:cerr
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf; 
  std::vector<std::string> builtins = {"echo" , "type" , "exit"};

  // REPL
  while(1){
    std::cout<<"$ ";
    std::string line;
    std::getline(std::cin,line);
    std::string command,args="";
    int ix=0,n=line.size();

    // if the line is empty
    if(line.empty()){
      continue;
    }

    // reading the command and the args
    while(ix<n){
      if(line[ix]==' '){
        break;
      }
      ix++;
    }

    command=line.substr(0,ix);
    if(ix!=n) args=line.substr(ix+1);

    // commands
    if(command == "exit"){
      break;
    }
    else if(command == "echo"){
      std::cout<<args<<std::endl;
    }
    else if(command == "type"){
      if(std::find(builtins.begin(),builtins.end(),args)!=builtins.end()){
        std::cout<<args<<" is a shell builtin"<<std::endl;
      }
      else{
        bool found=false;
        char* env = std::getenv("PATH");
        if(env == nullptr){
            std::cout<<args<<": not found"<<std::endl;
            continue;
        }
        std::string path = env;
        std::stringstream ss(path);
        std::string dir;

        #ifdef _WIN32
        const char delimiter = ';';
        #else
        const char delimiter = ':';
        #endif
        while(std::getline(ss,dir,delimiter)){
          std::filesystem::path pathStr = std::filesystem::path(dir)/args;
          if(std::filesystem::exists(pathStr)){
            if(access(pathStr.string().c_str(), X_OK) == 0){
              found=true;
              std::cout<<args<<" is "<<pathStr.string()<<std::endl;
            }
          }
        }
        if(!found) std::cout<<args<<": not found"<<std::endl;
      }
    }
    else{
      std::cout<<line<<": not found"<<std::endl;
    }
  }
}
