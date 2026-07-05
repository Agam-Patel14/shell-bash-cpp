#include <iostream>
#include <string>
#include <vector>
#include <cstdlib>
#include <sstream>
#include <filesystem>
#include <algorithm>
#include <cctype>
#include <cstring>
#include <unistd.h>
#include <sys/wait.h>
#include <system_error>
#include <fcntl.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <map>
#include <fstream>

struct parsedCommand {
  std::string command;
  std::vector<std::string> args;
  bool redirectStdout = false;
  bool redirectStderr = false;
  bool appendStdout = false;
  bool appendStderr = false;
  bool background = false;
  std::string outputFile;
  std::string errorFile;
};

std::map<std::string,std::string> completionsList;

struct Job {
  int jobNumber;
  pid_t pid;
  std::string command;
  bool running = true;
};

std::vector<Job> jobsList;

void checkJobs(bool isJobs){
  for(auto &job : jobsList){
    if(!job.running) continue;
    int status;
    pid_t result = waitpid(job.pid,&status,WNOHANG);
    if(result > 0){
      job.running = false;
    }
  }
  if(jobsList.size()!=0){
    int numJobs = jobsList.size();
    for(int i=0 ; i<numJobs ; i++){
      std::string marker = " ";
      if(i == numJobs-1) marker = "+";
      else if(i == numJobs-2) marker = "-";
      if(jobsList[i].running && isJobs){
        std::cout<<"["<<jobsList[i].jobNumber<<"]"<<marker<<"  Running                    "<<jobsList[i].command<<" &"<<std::endl;
      }
      else if(!jobsList[i].running){
        std::cout<<"["<<jobsList[i].jobNumber<<"]"<<marker<<"  Done                       "<<jobsList[i].command<<std::endl;
      }
    }
    jobsList.erase(std::remove_if(jobsList.begin(),jobsList.end(),
      [](const Job &j){ return !j.running; }),jobsList.end());
  }
}

std::vector<std::string> builtins = {"echo","type","exit","pwd","cd","complete","jobs","history"};

void echo(const std::vector<std::string> &args){
  for(int i=0;i<(int)args.size();i++){
    if(i) std::cout<<" ";
    std::cout<<args[i];
  }
  std::cout<<std::endl;
}

void pwd(){
  std::cout<<std::filesystem::current_path().string()<<std::endl;
}

void Type(const std::vector<std::string> &args){
  for(auto &arg : args){
    if(std::find(builtins.begin(),builtins.end(),arg)!=builtins.end()){
      std::cout<<arg<<" is a shell builtin"<<std::endl;
    }
    else{
      bool found = false;
      char* env = std::getenv("PATH");
      if(env){
        std::stringstream ss(env);
        std::string dir;
        while(std::getline(ss,dir,':')){
          std::string p = dir+"/"+arg;
          if(access(p.c_str(),X_OK)==0){
            found = true;
            std::cout<<arg<<" is "<<p<<std::endl;
            break;
          }
        }
      }
      if(!found) std::cout<<arg<<": not found"<<std::endl;
    }
  }
}

void cd(const std::vector<std::string> &args,const std::string &line){
  if(args.size() == 0){}
  else if(args.size()>2){
    std::cout<<"cd: too many arguments"<<std::endl;
  }
  else{
    std::string path = args[0];
    if(path == "~") path = getenv("HOME");
    std::error_code ec;
    std::filesystem::current_path(path,ec);
    if(ec) std::cout<<"cd:"<<line.substr(2)<<": No such file or directory"<<std::endl;
  }
}

void Complete(const std::vector<std::string> &args){
  if(args.size() == 0){}
  else if(args[0] == "-C"){
    if(args.size() > 2) completionsList[args[2]] = args[1];
  }
  else if(args[0] == "-p"){
    if(args.size() == 1){}
    else if(completionsList.find(args[1]) != completionsList.end()){
      std::cout<<"complete -C '"<<completionsList[args[1]]<<"' "<<args[1]<<std::endl;
    }
    else std::cout<<"complete: "<<args[1]<<": no completion specification"<<std::endl;
  }
  else if(args[0] == "-r"){
    if(args.size() == 1){}
    else if(completionsList.find(args[1]) != completionsList.end()){
      completionsList.erase(args[1]);
    }
  }
}

std::string getHistoryFile(){
  char* hf = std::getenv("HISTFILE");
  if(hf) return std::string(hf);
  char* home = std::getenv("HOME");
  if(home) return std::string(home) + "/.bash_history";
  return ".bash_history";
}

int historyLinesRead = 0;
int sessionStartOffset = 0;

void History(const std::vector<std::string> &args){
  std::string histFile = getHistoryFile();
  if(!args.empty() && args[0][0] == '-'){
    std::string flag = args[0];
    std::string targetFile = (args.size() > 1) ? args[1] : histFile;
    if(flag == "-c"){
      clear_history();
      sessionStartOffset = 0;
      return;
    }
    else if(flag == "-w"){
      write_history(targetFile.c_str());
      return;
    }
    else if(flag == "-a"){
      int total = history_length;
      int newEntries = total-sessionStartOffset;
      if(newEntries > 0){
        append_history(newEntries,targetFile.c_str());
        sessionStartOffset = total;
      }
      return;
    }
    else if(flag == "-n"){
      read_history_range(targetFile.c_str(),historyLinesRead,-1);
      std::ifstream hfile(targetFile);
      int count = 0;
      std::string tmp;
      while(std::getline(hfile,tmp)) count++;
      historyLinesRead = count;
      return;
    }
    else if(flag == "-r"){
      read_history(targetFile.c_str());
      std::ifstream hfile(targetFile);
      int count = 0;
      std::string tmp;
      while(std::getline(hfile,tmp)) count++;
      historyLinesRead = count;
      return;
    }
  }
  else{
    std::vector<std::string> lines;
    HIST_ENTRY **hist = history_list();
    if(hist){
      for(int i=0; hist[i] != nullptr; i++){
        lines.push_back(hist[i]->line);
      }
    }

    int num;
    if(args.size() == 0){
      num = lines.size();
    }
    else{
      num = std::atoi(args[0].c_str());
      if(num == 0){
        std::string nums = args[0];
        sort(nums.begin(),nums.end());
        if(nums[0] == nums[nums.size()-1] && nums[0] == '0'){
          num=0;
        }
        else{
          std::cout<<"history: "<<args[0]<<": numeric argument required"<<std::endl;
          return;
        }
      }
    }
    if(args.size()>1){
      std::cout<<"history: too many arguments"<<std::endl;
      return;
    }

    for(int i=(int)lines.size()-num ; i<(int)lines.size() ; i++){
      printf("%5d  ",i+1);
      std::cout<<lines[i]<<std::endl;
    }
  }
}

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
  std::vector<std::string> commands = {"echo","type","exit","pwd","cd","complete","jobs","history"};
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

std::vector<std::string> runCompleterScript(std::vector<std::string> args , const std::string compLine , int compPoint){
  std::string scriptPath = args[0];
  int pipefd[2];
  if(pipe(pipefd) == -1){
    perror("pipe");
    return {};
  }

  pid_t pid = fork();
  if(pid == 0){
    close(pipefd[0]);
    if(dup2(pipefd[1],STDOUT_FILENO) == -1){
      _exit(1);
    }
    close(pipefd[1]);
    std::vector<char*> argv;
    for(auto &s : args){
      argv.push_back(const_cast<char*>(s.c_str()));
    }
    argv.push_back(nullptr);
    std::string compPointStr = std::to_string(compPoint);
    setenv("COMP_LINE",compLine.c_str(),1);
    setenv("COMP_POINT",compPointStr.c_str(),1);
    execv(scriptPath.c_str(),argv.data());
    _exit(1);
  }

  if(pid < 0){
    perror("fork");
    close(pipefd[0]);
    close(pipefd[1]);
    return {};
  }

  close(pipefd[1]);
  std::string output;
  char buffer[4096];
  ssize_t bytesRead = 0;
  while((bytesRead = read(pipefd[0],buffer,sizeof(buffer))) > 0){
    output.append(buffer,bytesRead);
  }
  close(pipefd[0]);

  int status = 0;
  waitpid(pid,&status,0);
  if(!WIFEXITED(status) || WEXITSTATUS(status) != 0){
    return {};
  }

  std::vector<std::string> candidates;
  std::stringstream ss(output);
  std::string line;
  while(std::getline(ss,line)){
    if(!line.empty()) candidates.push_back(line);
  }
  return candidates;
}

char** commandCompletion(const char* text , int start , int end){
  if(start == 0){
    return rl_completion_matches(text,commandGenerator);
  }

  std::string buffer = rl_line_buffer;
  if(buffer.empty()){
    return nullptr;
  }
  std::string parsedLine = buffer.substr(0,rl_point);
  std::vector<std::string> args = parseArgs(parsedLine);
  if(args.empty()) return nullptr;
  if(completionsList.find(args[0]) == completionsList.end()) return nullptr;

  std::vector<std::string> argsc;
  argsc.push_back(completionsList[args[0]]);
  argsc.push_back(args[0]);
  argsc.push_back(args[args.size()-1]);
  if(args.size() >= 2) argsc.push_back(args[args.size()-2]);
  else argsc.push_back("");

  std::vector<std::string> candidate = runCompleterScript(argsc,buffer,rl_point);
  if(candidate.empty()) return nullptr;
  std::string completion = candidate[0];

  if(candidate.size() == 1){
    char** matches = static_cast<char**>(malloc(sizeof(char*)*2));
    matches[0] = strdup(completion.c_str());
    matches[1] = nullptr;
    return matches;
  }

  std::sort(candidate.begin(),candidate.end());
  std::string lcp = completion;
  for(size_t i=1 ; i<candidate.size() ; i++){
    size_t j=0;
    while(j<lcp.size() && j<candidate[i].size() && lcp[j]==candidate[i][j]) j++;
    lcp = lcp.substr(0,j);
  }

  char** matches = static_cast<char**>(malloc(sizeof(char*)*(candidate.size()+2)));
  matches[0] = strdup(lcp.c_str());
  for(size_t i=0 ; i<candidate.size() ; i++){
    matches[i+1] = strdup(candidate[i].c_str());
  }
  matches[candidate.size()+1] = nullptr;
  return matches;
}

int main() {
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;

  std::string histFile = getHistoryFile();
  read_history(histFile.c_str());
  sessionStartOffset = history_length;

  rl_attempted_completion_function = commandCompletion;
  // REPL
  while(1){
    checkJobs(false);
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

    bool hasPipe = false;
    for(auto &a : arguments) if(a == "|"){
      hasPipe = true;
      break;
    }

    if(hasPipe){
      std::vector<std::vector<std::string>> segments;
      std::vector<std::string> curr;
      for(auto &a : arguments){
        if(a == "|"){
          if(!curr.empty()) segments.push_back(curr);
          curr.clear();
        }
        else{
          curr.push_back(a);
        }
      }
      if(!curr.empty()) segments.push_back(curr);

      int numSegments = segments.size();
      std::vector<int> pipefds(2*(numSegments-1));
      for(int i=0;i<numSegments-1;i++){
        if(pipe(&pipefds[2*i]) == -1){
          perror("pipe");
          break;
        }
      }

      std::vector<pid_t> pids;
      for(int i=0 ; i<numSegments ; i++){
        pid_t pid = fork();
        if(pid == 0){
          if(i > 0){
            if(dup2(pipefds[2*(i-1)],STDIN_FILENO) == -1){
              perror("dup2");
              _exit(1);
            }
          }
          if(i < numSegments-1){
            if(dup2(pipefds[2*i+1],STDOUT_FILENO) == -1){
              perror("dup2");
              _exit(1);
            }
          }
          for(int j=0 ; j<(int)pipefds.size() ; j++) close(pipefds[j]);

          std::string cmd = segments[i][0];
          std::vector<std::string> segArgs(segments[i].begin()+1,segments[i].end());

          if(cmd == "echo"){
            echo(segArgs);
            _exit(0);
          }
          else if(cmd == "pwd"){
            pwd();
            _exit(0);
          }
          else if(cmd == "cd"){
            _exit(0);
          }
          else if(cmd == "complete"){
            Complete(segArgs);
            _exit(0);
          }
          else if(cmd == "jobs"){
            checkJobs(true);
            _exit(0);
          }
          else if(cmd == "history"){
            History(segArgs);
            _exit(0);
          }
          else if(cmd == "type"){
            Type(segArgs);
            _exit(0);
          }
          else if(cmd == "exit"){
            _exit(0);
          }
          else{
            std::vector<const char*> argv;
            for(auto &s : segments[i]) argv.push_back(s.c_str());
            argv.push_back(nullptr);
            execvp(argv[0],const_cast<char**>(argv.data()));
            std::cerr<<segments[i][0]<<": not found"<<std::endl;
            _exit(1);
          } 
        }
        else if(pid > 0){
          pids.push_back(pid);
        }
        else{
          perror("fork");
        }
      }

      for(int j=0 ; j<(int)pipefds.size() ; j++) close(pipefds[j]);

      for(auto p : pids){
        int status;
        waitpid(p,&status,0);
      }
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
    if(!input.args.empty() && input.args.back() == "&"){
      input.background = true;
      input.args.pop_back();
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
      echo(input.args);
    }
    else if(input.command == "pwd"){
      pwd();
    }
    else if(input.command == "cd"){
      cd(input.args,line);
    }
    else if(input.command == "complete"){
      Complete(input.args);
    }
    else if(input.command == "jobs"){
      checkJobs(true);
    }
    else if(input.command == "history"){
      History(input.args);
    }
    else if(input.command == "type"){
      Type(input.args);
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
        if(input.background){
          int nextJobNumber = 1;
          if(!jobsList.empty()){
            int maxNum = 0;
            for(auto &j : jobsList) if(j.jobNumber > maxNum) maxNum = j.jobNumber;
            nextJobNumber = maxNum + 1;
          }
          std::cout<<"["<<nextJobNumber<<"] "<<pid<<std::endl;
          Job job;
          job.jobNumber = nextJobNumber;
          job.pid = pid;
          job.command = line;
          if(job.command.size() >= 2 && job.command.substr(job.command.size()-2) == " &"){
            job.command = job.command.substr(0,job.command.size()-2);
          }
          jobsList.push_back(job);
        }
        else{
          int status;
          waitpid(pid,&status,0);
        }
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