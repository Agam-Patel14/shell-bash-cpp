#include <iostream>
#include <string>
#include <vector>

int main() {
  // Flush after every std::cout / std:cerr
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf; 

  // REPL
  while(1){
    std::cout<<"$ ";
    std::string command;
    std::getline(std::cin,command);
    std::vector<std::string> arr = {"echo" , "type" , "exit"};

    // commands
    if(command == "exit"){
      break;
    }
    else if(command.substr(0,4) == "echo"){
      std::cout<<command.substr(5)<<std::endl;
    }
    else if(command.substr(0,4) == "type"){
      bool found=false;
      for(auto x : arr){
        if(command.substr(5) == x){
          found=true;
          std::cout<<x<<" is a shell builtin"<<std::endl;
          break;
        }
      }
      if(!found){
        std::cout<<command.substr(5)<<": not found"<<std::endl;
      }
    }
    else{
      std::cout<<command<<": not found"<<std::endl;
    }
  }
}
