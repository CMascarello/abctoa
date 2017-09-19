/*Alembic Holder
Copyright (c) 2014, Gaël Honorez, All rights reserved.
This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 3.0 of the License, or (at your option) any later version.
This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
Lesser General Public License for more details.
You should have received a copy of the GNU Lesser General Public
License along with this library.*/

#pragma once
#define H_VAL(str) #str
#define H_TOSTRING(str) H_VAL(str)

#define HOLDER_VENDOR "Nozon"
#define HOLDER_VERSION_NUM 2.2.1


#define ARCH_VERSION         H_TOSTRING(HOLDER_VERSION_NUM) 

#ifndef MAYA_VERSION
   #define MAYA_VERSION 2017
#endif
