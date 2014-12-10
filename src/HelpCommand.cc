/* -*- Mode: C++; tab-width: 8; c-basic-offset: 2; indent-tabs-mode: nil; -*- */

#include "Command.h"

using namespace std;

class HelpCommand : public Command {
public:
  virtual int run(std::vector<std::string>& args);

protected:
  HelpCommand(const char* name, const char* help) : Command(name, help) {}

  static HelpCommand help1;
  static HelpCommand help2;
  static HelpCommand help3;
};

HelpCommand HelpCommand::help1("help", " rr help [command]\n");
HelpCommand HelpCommand::help2("-h", nullptr);
HelpCommand HelpCommand::help3("--help", nullptr);

int HelpCommand::run(std::vector<std::string>& args) {
  if (args.size() == 0) {
    return print_usage();
  }

  Command* command = Command::command_for_name(args[0]);
  if (!command) {
    return print_usage();
  }
  command->print_help(stderr);
  return 0;
}
