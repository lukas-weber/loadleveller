#pragma once

#include <functional>
#include "mc.h"

int merge(function<abstract_mc* (string&)> mccreator,int argc, char *argv[]);
