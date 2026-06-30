#include <iostream>
#include <string>
#include <vector>

int main() {
  // Flush after every std::cout / std:cerr
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf; 
  std::vector<std::string> arr = {"echo" , "type" , "exit"};

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
      bool found=false;
      for(auto &x : arr){
        if(args == x){
          found=true;
          std::cout<<x<<" is a shell builtin"<<std::endl;
          break;
        }
      }
      if(!found){
        std::cout<<args<<": not found"<<std::endl;
      }
    }
    else{
      std::cout<<line<<": not found"<<std::endl;
    }
  }
}
