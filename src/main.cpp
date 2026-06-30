#include <iostream>
#include <string>

int main() {
  // Flush after every std::cout / std:cerr
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf; 

  // REPL
  while(1){
    std::cout<<"$ ";
    std::string command;
    std::getline(std::cin,command);

    // commands
    if(command == "exit"){
      break;
    }

    std::cout<<command<<": not found"<<std::endl;
  }
}
