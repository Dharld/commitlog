#pragma once
#include "object_store.hpp"
#include <memory>
#include <string>


// Minimal interface that all commands implement
struct ICommand {
    virtual ~ICommand() = default;
    virtual int execute(int argc, char** argv, ObjectStore& store) = 0;
    virtual const char* name() const = 0;
};

// Factory defined in commands.cpp
std::unique_ptr<ICommand> make_command(const std::string& name);
