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
%% A simple boot_server at a CP.
%%
%% This server should know about which slaves (DP's or whatever) to boot.
%% File's (with absolute path name) will be fetched.
%%

-module(erl_boot_server).
-moduledoc """
Boot server for other Erlang machines.

This server is used to assist diskless Erlang nodes that fetch all Erlang code
from another machine.

This server is used to fetch all code, including the start script, if an Erlang
runtime system is started with command-line flag `-loader inet`. All hosts
specified with command-line flag `-hosts Host` must have one instance of this
server running.

This server can be started with the Kernel configuration parameter
`start_boot_server`.

The `erl_boot_server` can read regular files and files in archives. See `m:code`
and `m:erl_prim_loader` in ERTS.

> #### Warning {: .warning }
>
> The support for loading code from archive files is experimental. It is
> released before it is ready to obtain early feedback. The file format,
> semantics, interfaces, and so on, can be changed in a future release.

## SEE ALSO

[`erts:init`](`m:init`), [`erts:erl_prim_loader`](`m:erl_prim_loader`)
""".

-compile(nowarn_deprecated_catch).

-include("inet_boot.hrl").

-behaviour(gen_server).

%% API functions.
-export([start/1, start/2,
         start_link/1, start_link/2,
         add_slave/1, delete_slave/1,
         add_subnet/2, delete_subnet/2,
         which_slaves/0]).

%% Exports for testing (don't remove; tests suites depend on them).
-export([would_be_booted/1]).

%% Internal exports
-export([init/1,handle_call/3,handle_cast/2,handle_info/2,terminate/2]).
-export([code_change/3]).
-export([boot_init/1, boot_accept/3]).

-record(state, 
	{
	  priority = 0,  %% priority of this server
	  version = ""   :: string(),	%% Version handled i.e "4.5.3" etc
	  udp_sock,      %% listen port for broadcast requests
	  udp_port,      %% port number must be ?EBOOT_PORT!
	  listen_sock,   %% listen sock for incoming file requests
	  listen_port,   %% listen port number
	  slaves,        %% list of accepted ip addresses
	  bootp          :: pid(),	%% boot process
	  prim_state     %% state for efile code loader
	 }).
-type state() :: #state{}.

-define(single_addr_mask, {255, 255, 255, 255}).

-doc """
The same as [`start(Slaves, #{})`](`start/2`).
""".
-spec start(Slaves) -> {'ok', Pid} | {'error', Reason} when
      Slaves :: [Host],
      Host :: inet:ip_address() | inet:hostname(),
      Pid :: pid(),
      Reason :: any().

start(Slaves) ->
    start(Slaves, #{}).

-doc """
Starts the boot server. `Slaves` is a list of IP addresses for hosts, which are
allowed to use this server as a boot server. `Options` is a map with
configuration options.

The boot server listening port can be configured with `listen_port`.
If an empty map is provided, or `listen_port` is zero, then an ephemeral port
is used.
""".
-spec start(Slaves, Options) -> {'ok', Pid} | {'error', Reason} when
      Slaves :: [Host],
      Host :: inet:ip_address() | inet:hostname(),
      Options :: #{listen_port => inet:port_number()},
      Pid :: pid(),
      Reason :: any().

start(Slaves, Options) ->
    case start_args(Slaves, Options) of
        {ok, StartArgs} ->
            gen_server:start({local,boot_server}, erl_boot_server, StartArgs, []);
        {error, _} = Error ->
            Error
    end.

-doc """
The same as [`start_link(Slaves, #{})`](`start_link/2`).
""".
-spec start_link(Slaves) -> {'ok', Pid} | {'error', Reason} when
      Slaves :: [Host],
      Host :: inet:ip_address() | inet:hostname(),
      Pid :: pid(),
      Reason :: any().

start_link(Slaves) ->
    start_link(Slaves, #{}).

-doc """
The same as [`start(Slaves, Options)`](`start/2`), but it also links to the
caller.
""".
-spec start_link(Slaves, Options) -> {'ok', Pid} | {'error', Reason} when
      Slaves :: [Host],
      Host :: inet:ip_address() | inet:hostname(),
      Options :: #{listen_port => inet:port_number()},
      Pid :: pid(),
      Reason :: any().

start_link(Slaves, Options) ->
    case start_args(Slaves, Options) of
        {ok, StartArgs} ->
            gen_server:start_link({local,boot_server}, erl_boot_server, StartArgs, []);
        {error, _} = Error ->
            Error
    end.

start_args(Slaves, Options) ->
    case check_arg(Slaves) of
        {ok, Arg} ->
            case check_options(Options) of
                true ->
                    NewOptions = with_default_options(Options),
                    ListenPort = maps:get(listen_port, NewOptions),
                    {ok, #{slaves => Arg, listen_port => ListenPort}};
                false ->
                    {error, {badarg, Options}}
            end;
        _ ->
            {error, {badarg, Slaves}}
    end.

check_arg(Slaves) ->
    check_arg(Slaves, []).

check_arg([Slave|Rest], Result) ->
    case inet:getaddr(Slave, inet) of
	{ok, IP} ->
	    check_arg(Rest, [{?single_addr_mask, IP}|Result]);
	_ ->
	    error
    end;
check_arg([], Result) ->
    {ok, Result};
check_arg(_, _Result) ->
    error.

check_options(Options) when is_map(Options) ->
    lists:all(fun valid_option/1, maps:to_list(Options));
check_options(_) ->
    false.

valid_option({listen_port, Port}) when is_integer(Port) -> true;
valid_option({_, _}) -> false.

with_default_options(Options) ->
    DefaultOptions = #{listen_port => 0},
    maps:merge(DefaultOptions, Options).

-doc "Adds a `Slave` node to the list of allowed slave hosts.".
-spec add_slave(Slave) -> 'ok' | {'error', Reason} when
      Slave :: Host,
      Host :: inet:ip_address() | inet:hostname(),
      Reason :: {'badarg', Slave}.

add_slave(Slave) ->
    case inet:getaddr(Slave, inet) of
	{ok,IP} ->
	    gen_server:call(boot_server, {add, {?single_addr_mask, IP}});
	_ ->
	    {error, {badarg, Slave}}
    end.

-doc "Deletes a `Slave` node from the list of allowed slave hosts.".
-spec delete_slave(Slave) -> 'ok' | {'error', Reason} when
      Slave :: Host,
      Host :: inet:ip_address() | inet:hostname(),
      Reason :: {'badarg', Slave}.

delete_slave(Slave) ->
    case inet:getaddr(Slave, inet) of
	{ok,IP} ->
	    gen_server:call(boot_server, {delete, {?single_addr_mask, IP}});
	_ ->
	    {error, {badarg, Slave}}
    end.

-doc false.
-spec add_subnet(Netmask :: inet:ip_address(), Addr :: inet:ip_address()) ->
	'ok' | {'error', any()}.

add_subnet(Mask, Addr) when is_tuple(Mask), is_tuple(Addr) ->
    case member_address(Addr, [{Mask, Addr}]) of
	true ->
	    gen_server:call(boot_server, {add, {Mask, Addr}});
	false ->
	    {error, empty_subnet}
    end.

-doc false.
-spec delete_subnet(Netmask :: inet:ip_address(),
                    Addr :: inet:ip_address()) -> 'ok'.

delete_subnet(Mask, Addr) when is_tuple(Mask), is_tuple(Addr) ->
    gen_server:call(boot_server, {delete, {Mask, Addr}}).

-doc "Returns the current list of allowed slave hosts.".
-spec which_slaves() -> Slaves when
      Slaves :: [Slave],
      Slave :: {Netmask :: inet:ip_address(), Address :: inet:ip_address()}.

which_slaves() ->
    gen_server:call(boot_server, which).

%% Given a host name or IP address, returns true if a host
%% having that IP address would be accepted for booting, and
%% false otherwise.  (Convenient for testing.)

-doc false.
would_be_booted(Addr) ->
    {ok, IP} = inet:getaddr(Addr, inet),
    member_address(IP, which_slaves()).

int16(X) when is_integer(X) ->
    [(X bsr 8) band 16#ff, (X) band 16#ff].

%% Check if an address is a member

member_address(IP, [{{MA, MB, MC, MD}, {EA, EB, EC, ED}}|Rest]) ->
    {A, B, C, D} = IP,
    if A band MA =:= EA,
       B band MB =:= EB,
       C band MC =:= EC,
       D band MD =:= ED ->
	    true;
       true ->
	    member_address(IP, Rest)
    end;
member_address(_, []) ->
    false.

%% ------------------------------------------------------------
%% call-back functions.
%% ------------------------------------------------------------

-doc false.
-spec init(#{slaves      := list(),
             listen_port := inet:port_number()})
          -> {'ok', state()}.

init(#{slaves      := Slaves,
       listen_port := ListenPort}) ->
    {ok, U} = gen_udp:open(?EBOOT_PORT, []),
    {ok, L} = gen_tcp:listen(ListenPort, [binary,{packet,4}]),
    {ok, Port} = inet:port(L),
    {ok, UPort} = inet:port(U),
    Ref = make_ref(),
    Pid = proc_lib:spawn_link(?MODULE, boot_init, [Ref]),
    ok = gen_tcp:controlling_process(L, Pid),
    Pid ! {Ref, L},
    %% We trap exit inorder to restart boot_init and udp_port 
    process_flag(trap_exit, true),
    {ok, #state{priority = 0,
		version = erlang:system_info(version),
		udp_sock = U,
		udp_port = UPort,
		listen_sock = L,
		listen_port = Port,
		slaves = ordsets:from_list(Slaves),
		bootp = Pid
	       }}.

-doc false.
-spec handle_call('which' | {'add',atom()} | {'delete',atom()}, _, state()) ->
        {'reply', 'ok' | [atom()], state()}.

handle_call({add,Address}, _, S0) ->
    Slaves = ordsets:add_element(Address, S0#state.slaves),
    S0#state.bootp ! {slaves, Slaves},
    {reply, ok, S0#state{slaves = Slaves}};
handle_call({delete,Address}, _, S0) ->
    Slaves = ordsets:del_element(Address, S0#state.slaves),
    S0#state.bootp ! {slaves, Slaves},
    {reply, ok, S0#state{slaves = Slaves}};
handle_call(which, _, S0) ->
    {reply, ordsets:to_list(S0#state.slaves), S0}.

-doc false.
-spec handle_cast(term(), [atom()]) -> {'noreply', [atom()]}.

handle_cast(_, Slaves) ->
    {noreply, Slaves}.

-doc false.
-spec handle_info(term(), state()) -> {'noreply', state()}.

handle_info({udp, U, IP, Port, Data}, S0) ->
    Token = ?EBOOT_REQUEST ++ S0#state.version,
    Valid = member_address(IP, ordsets:to_list(S0#state.slaves)),
    %% check that the connecting node is valid and has the same
    %% erlang version as the boot server node
    case {Valid,Data,Token} of
	{true,Token,Token} ->
	    case gen_udp:send(U,IP,Port,[?EBOOT_REPLY,S0#state.priority,
                                         int16(S0#state.listen_port),
                                         S0#state.version])
            of
                ok -> ok;
                {error, not_owner} ->
                    error_logger:error_msg("** Illegal boot server connection attempt: "
				   "not owner of ~w ** ~n", [U]);
                {error, Reason} ->
                    Err = file:format_error(Reason),
                    error_logger:error_msg("** Illegal boot server connection attempt: "
				   "~w POSIX error ** ~n", [U, Err])
            end,
	    {noreply,S0};
	{false,_,_} ->
	    error_logger:error_msg("** Illegal boot server connection attempt: "
				   "~w is not a valid address ** ~n", [IP]),
	    {noreply,S0};
	{true,_,_} ->
	    case catch string:slice(Data, 0, length(?EBOOT_REQUEST)) of
		?EBOOT_REQUEST ->
		    Vsn = string:slice(Data, length(?EBOOT_REQUEST), length(Data)),
		    error_logger:error_msg("** Illegal boot server connection attempt: "
					   "client version is ~s ** ~n", [Vsn]);
		_ ->
		    error_logger:error_msg("** Illegal boot server connection attempt: "
					   "unrecognizable request ** ~n", [])
	    end,
	    {noreply,S0}
    end;
handle_info(_Info, S0) ->
    {noreply,S0}.

-doc false.
-spec terminate(term(), state()) -> 'ok'.

terminate(_Reason, _S0) ->
    ok.

-doc false.
-spec code_change(term(), state(), term()) -> {'ok', state()}.

code_change(_Vsn, State, _Extra) ->
    {ok, State}.

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%%
%% Boot server 
%%
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

-doc false.
-spec boot_init(reference()) -> no_return().

boot_init(Tag) ->
    receive
	{Tag, Listen} ->
	    process_flag(trap_exit, true),
	    boot_main(Listen)
    end.

boot_main(Listen) ->
    Tag = make_ref(),
    Pid = proc_lib:spawn_link(?MODULE, boot_accept, [self(), Listen, Tag]),
    boot_main(Listen, Tag, Pid).

boot_main(Listen, Tag, Pid) ->
    receive
	{Tag, _} ->
	    boot_main(Listen);
	{'EXIT', Pid, _} -> 
	    boot_main(Listen);
	{'EXIT', _, Reason} ->
	    exit(Pid, kill),
	    exit(Reason);
	{tcp_closed, Listen} ->
	    exit(closed)
    end.

-doc false.
boot_accept(Server, Listen, Tag) ->
    Reply = gen_tcp:accept(Listen),
    unlink(Server),
    Server ! {Tag, continue},
    case Reply of
	{ok, Socket} ->
	    {ok, {IP, _Port}} = inet:peername(Socket),
	    true = member_address(IP, which_slaves()),
	    PS = erl_prim_loader:prim_init(),
	    boot_loop(Socket, PS)
    end.

boot_loop(Socket, PS) ->
    receive
	{tcp, Socket, Data} ->
	    PS2 = handle_command(Socket, PS, Data),
	    boot_loop(Socket, PS2);
	{tcp_closed, Socket} ->
	    true
    end.

handle_command(S, PS, Msg) ->
    case catch binary_to_term(Msg) of
	{get,File} ->
	    {Res, PS2} = erl_prim_loader:prim_read_file(PS, File),
	    send_file_result(S, get, Res),
	    PS2;
	{list_dir,Dir} ->
	    {Res, PS2} = erl_prim_loader:prim_list_dir(PS, Dir),
	    send_file_result(S, list_dir, Res),
	    PS2;
	{read_file_info,File} ->
	    {Res, PS2} = erl_prim_loader:prim_read_file_info(PS, File, true),
	    send_file_result(S, read_file_info, Res),
	    PS2;
        {read_link_info,File} ->
            {Res, PS2} = erl_prim_loader:prim_read_file_info(PS, File, false),
            send_file_result(S, read_link_info, Res),
            PS2;
	get_cwd ->
	    {Res, PS2} = erl_prim_loader:prim_get_cwd(PS, []),
	    send_file_result(S, get_cwd, Res),
	    PS2;
	{get_cwd,Drive} ->
	    {Res, PS2} = erl_prim_loader:prim_get_cwd(PS, [Drive]),
	    send_file_result(S, get_cwd, Res),
	    PS2;
	{'EXIT',Reason} ->
	    send_result(S, {error,Reason}),
	    PS;
	_Other ->
	    send_result(S, {error,unknown_command}),
	    PS
    end.

send_file_result(S, Cmd, Result) ->
    send_result(S, {Cmd,Result}).

send_result(S, Term) ->
    case gen_tcp:send(S, term_to_binary(Term)) of
	ok ->
	    ok;
	Error ->
	    error_logger:error_msg("** Boot server could not send result "
				   "to socket: ~w** ~n", [Error]),
	    ok
    end.
