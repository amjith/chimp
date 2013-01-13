/*****************************************************************************
 *                                                                           *
 * Copyright 2012 Thomas Lee                                                 *
 *                                                                           *
 * Licensed under the Apache License, Version 2.0 (the "License");           *
 * you may not use this file except in compliance with the License.          *
 * You may obtain a copy of the License at                                   *
 *                                                                           *
 *     http://www.apache.org/licenses/LICENSE-2.0                            *
 *                                                                           *
 * Unless required by applicable law or agreed to in writing, software       *
 * distributed under the License is distributed on an "AS IS" BASIS,         *
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  *
 * See the License for the specific language governing permissions and       *
 * limitations under the License.                                            *
 *                                                                           *
 *****************************************************************************/

#ifndef _CHIMP_FLOAT_H_INCLUDED_
#define _CHIMP_FLOAT_H_INCLUDED_

#include <chimp/any.h>

#ifdef __cplusplus
extern "C" { 
#endif

typedef struct _ChimpFloat { 
    ChimpAny base;
    double value;
} ChimpFloat;

chimp_bool_t
chimp_float_class_bootstrap (void);

ChimpRef *
chimp_float_new (double value);

#define CHIMP_FLOAT(ref) CHIMP_CHECK_CAST(ChimpFloat, (ref), chimp_float_class)

#define CHIMP_FLOAT_VALUE(ref) CHIMP_FLOAT(ref)->value

CHIMP_EXTERN_CLASS(float);

#ifdef __cplusplus
};
#endif

#endif
