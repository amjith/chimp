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

ChimpRef *chimp_float_class = NULL;

static ChimpRef *
_chimp_float_init (ChimpRef *self, ChimpRef *args)
{
    /* TODO convert str arg to int */
    return self;
}

chimp_bool_t
chimp_float_class_bootstrap (void)
{
    chimp_float_class = 
        chimp_class_new (CHIMP_STR_NEW ("float"), NULL, sizeof(ChimpFloat));
    if (chimp_float_class == NULL) {
        return CHIMP_FALSE;
    }
    chimp_gc_make_root (NULL, chimp_float_class);
    CHIMP_CLASS(chimp_float_class)->init = _chimp_float_init;
    return CHIMP_TRUE;
}

ChimpRef* 
chimp_float_new (double value)
{
    ChimpRef *ref = chimp_class_new_instance (chimp_float_class, NULL);
    CHIMP_FLOAT(ref)->value = value;
    return ref;
}
