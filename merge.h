#pragma once

#include <functional>
#include "mc.h"

int merge(function<mc* (string&)> mccreator,int argc, char *argv[]);
