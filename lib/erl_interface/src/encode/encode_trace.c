/*
 * %CopyrightBegin%
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright Ericsson AB 1998-2025. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * %CopyrightEnd%
 */
#include "eidef.h"
#include "putget.h"

int ei_encode_trace(char *buf, int *index, const erlang_trace *p)
{
  /* { Flags, Label, Serial, FromPid, Prev } */
    if (ei_encode_tuple_header(buf,index,5) < 0)
        return -1;
    if (ei_encode_long(buf,index,p->flags) < 0)
        return -1;
    if (ei_encode_long(buf,index,p->label) < 0)
        return -1;
    if (ei_encode_long(buf,index,p->serial) < 0)
        return -1;
    if (ei_encode_pid(buf,index,&p->from) < 0)
        return -1;
    if (ei_encode_long(buf,index,p->prev) < 0)
        return -1;

  /* index is updated by the functions we called */
  
  return 0;
}

