%%
%% %CopyrightBegin%
%%
%% SPDX-License-Identifier: Apache-2.0
%%
%% Copyright Ericsson AB 1996-2025. All Rights Reserved.
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

-module(erl_signal_handler).
-moduledoc false.
-behaviour(gen_event).
-export([start/0, init/1,
         handle_event/2, handle_call/2, handle_info/2,
         terminate/2, code_change/3]).

-record(state,{}).

start() ->
    %% add signal handler
    gen_event:add_handler(erl_signal_server, erl_signal_handler, []).

init(_Args) ->
    {ok, #state{}}.

handle_event(sigusr1, S) ->
    erlang:halt("Received SIGUSR1"),
    {ok, S};
handle_event(sigquit, S) ->
    erlang:halt(),
    {ok, S};
handle_event(sigterm, S) ->
    error_logger:info_msg("SIGTERM received - shutting down~n"),
    ok = init:stop(),
    {ok, S};
handle_event(_SignalMsg, S) ->
    {ok, S}.

handle_info(_Info, S) ->
    {ok, S}.

handle_call(_Request, S) ->
    {ok, ok, S}.

code_change(_OldVsn, S, _Extra) ->
    {ok, S}.

terminate(_Args, _S) ->
    ok.
