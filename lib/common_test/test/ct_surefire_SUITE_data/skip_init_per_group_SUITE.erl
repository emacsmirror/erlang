%% %CopyrightBegin%
%%
%% SPDX-License-Identifier: Apache-2.0
%%
%% Copyright Ericsson AB 2016-2025. All Rights Reserved.
%%
%% Licensed under the Apache License, Version 2.0 (the "License");
%% you may not use this file except in compliance with the License.
%% You may obtain a copy of the License at
%%
%%     http://www.apache.org/licenses/LICENSE-2.0
%%
%% Unless required by applicable law or agreed to in writing, software
%% distributed under the License is distributed on an "AS IS" BASIS,
%% WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
%% See the License for the specific language governing permissions and
%% limitations under the License.
%%
%% %CopyrightEnd%
%%
-module(skip_init_per_group_SUITE).
-include_lib("common_test/include/ct.hrl").

-export([all/0, test_case/1]).
-compile([export_all, nowarn_export_all]).

init_per_group(left, _Config) ->
    {skip, skip_on_purpose};
init_per_group(_, Config) ->
    Config.

end_per_group(_GroupName, Config) ->
    Config.

all() ->
    [{group, root}].

groups() ->
    [{root, [], [{group, left}, {group, right}]},
     {left, [], [test_case, {group, nested_group}]},
     {nested_group, [], [test_case]},
     {right, [], [test_case]}].

test_case(_Config) ->
    ok.
