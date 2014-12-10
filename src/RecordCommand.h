/* -*- Mode: C++; tab-width: 8; c-basic-offset: 2; indent-tabs-mode: nil; -*- */

#ifndef RR_RECORD_COMMAND_H_
#define RR_RECORD_COMMAND_H_

#include "Command.h"

class RecordCommand : public Command {
public:
  virtual int run(std::vector<std::string>& args);

  static RecordCommand* get() { return &singleton; }

protected:
  RecordCommand(const char* name, const char* help) : Command(name, help) {}

  static RecordCommand singleton;
};

#endif // RR_RECORD_COMMAND_H_
