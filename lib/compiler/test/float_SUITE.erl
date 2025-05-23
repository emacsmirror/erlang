%%
%% %CopyrightBegin%
%%
%% SPDX-License-Identifier: Apache-2.0
%%
%% Copyright Ericsson AB 2002-2025. All Rights Reserved.
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
-module(float_SUITE).
-export([all/0, suite/0,groups/0,init_per_suite/1, end_per_suite/1, 
	 init_per_group/2,end_per_group/2,
	 pending/1,bif_calls/1,math_functions/1,mixed_float_and_int/1,
         subtract_number_type/1,float_followed_by_guard/1,
         fconv_line_numbers/1,float_zero/1,exception_signals/1]).

-include_lib("common_test/include/ct.hrl").

suite() -> [{ct_hooks,[ts_install_cth]}].

all() ->
    [pending, bif_calls, math_functions, float_zero,
     mixed_float_and_int, subtract_number_type,
     float_followed_by_guard,fconv_line_numbers,
     exception_signals].

groups() -> 
    [].

init_per_suite(Config) ->
    test_lib:recompile(?MODULE),
    Config.

end_per_suite(_Config) ->
    ok.

init_per_group(_GroupName, Config) ->
    Config.

end_per_group(_GroupName, Config) ->
    Config.

float_zero(Config) when is_list(Config) ->
    <<16#0000000000000000:64>> = match_on_zero_and_to_binary(1*0.0),
    <<16#8000000000000000:64>> = match_on_zero_and_to_binary(-1*0.0),
    ok.

match_on_zero_and_to_binary(X) when X == 0.0 -> <<X/float>>.

%% Thanks to Tobias Lindahl <tobias.lindahl@it.uu.se>
%% Shows the effect of pending exceptions on the x86.

pending(Config) when is_list(Config) ->
    case catch float_mul(1, 1.1e300, 3.14e300) of
	{'EXIT',{badarith,_}} -> ok;
	Other -> ct:fail({expected_exception,Other})
    end,
    0.0 = float_sub(2.0).

float_sub(A)->
    catch A - 2.0.

float_mul(0, _, _)->
    ok;
float_mul(Iter, A, B) when is_float(A), is_float(B) ->
    _ = A*B,
    float_mul(Iter-1, A, B).

%% Thanks to Mikael Pettersson and Tobias Lindahl (HiPE).

bif_calls(Config) when is_list(Config) ->
    {'EXIT',{badarith,_}} = (catch bad_arith(2.0, 1.7)),
    {'EXIT',{badarith,_}} = (catch bad_arith_again(2.0, [])),
    {'EXIT',{badarith,_}} = (catch bad_arith_xor(2.0, [])),
    {'EXIT',{badarith,_}} = (catch bad_arith_hd(2.0, [])),
    {'EXIT',{badarith,_}} = (catch bad_negate(2.0, 1.7)),
    ok.

bad_arith(X, Y) when is_float(X) ->
    X1 = X * 1.7e+308,
    X2 = X1 + 1.0,
    Y1 = Y * 2,					%Calls erts_mixed_times/2.
						%(A BIF call.)
    {X2, Y1}.

bad_arith_xor(X, Y) when is_float(X) ->
    X1 = X * 1.7e+308,
    Y1 = Y xor true,				%A failing BIF call.
    {X1 + 1.0, Y1}.

bad_arith_hd(X, Y) when is_float(X) ->
    X1 = X * 1.7e+308,
    Y1 = hd(Y),					%A failing BIF call.
    {X1 + 1.0, Y1}.

bad_arith_again(X, Y) when is_float(X) ->
    X1 = X * 1.7e+308,
    Y1 = element(1, Y),				%A failing BIF call.
    {X1 + 1.0, Y1}.

bad_negate(X, Y) when is_float(X) ->
    X1 = X * 1.7e+308,
    X2 = X1 + 1.0,
    Y1 = -Y,					%BIF call.
    {X2, Y1}.

%% Some math functions are not implemented on all platforms.
-define(OPTIONAL(Expected, Expr),
	try
	    Expected = Expr
	catch
	    error:undef -> ok
	end).

math_functions(Config) when is_list(Config) ->
    %% Mostly silly coverage.
    0.0 = math:tan(0),
    0.0 = math:atan2(0, 1),
    0.0 = math:sinh(0),
    1.0 = math:cosh(0),
    0.0 = math:tanh(0),
    1.0 = math:log2(2),
    1.0 = math:log10(10),
    -1.0 = math:cos(math:pi()),
    1.0 = math:exp(0),
    1.0 = math:pow(math:pi(), 0),
    0.0 = math:log(1),
    0.0 = math:asin(0),
    0.0 = math:acos(1),
    ?OPTIONAL(0.0, math:asinh(0)),
    ?OPTIONAL(0.0, math:acosh(1)),
    ?OPTIONAL(0.0, math:atanh(0)),
    ?OPTIONAL(0.0, math:erf(0)),
    ?OPTIONAL(1.0, math:erfc(0)),

    0.0 = math:tan(id(0)),
    0.0 = math:atan2(id(0), 1),
    0.0 = math:sinh(id(0)),
    1.0 = math:cosh(id(0)),
    0.0 = math:tanh(id(0)),
    1.0 = math:log2(id(2)),
    1.0 = math:log10(id(10)),
    1.0 = math:exp(id(0)),
    0.0 = math:log(id(1)),
    0.0 = math:asin(id(0)),
    0.0 = math:acos(id(1)),
    ?OPTIONAL(0.0, math:asinh(id(0))),
    ?OPTIONAL(0.0, math:acosh(id(1))),
    ?OPTIONAL(0.0, math:atanh(id(0))),
    ?OPTIONAL(0.0, math:erf(id(0))),
    ?OPTIONAL(1.0, math:erfc(id(0))),

    5.0 = math:floor(5.6),
    6.0 = math:ceil(5.6),
    5.0 = math:floor(id(5.4)),
    6.0 = math:ceil(id(5.4)),

    0.0 = math:fmod(42, 42),
    0.25 = math:fmod(1, 0.75),
    -1.0 = math:fmod(-4.0, 1.5),
    -0.375 = math:fmod(-3.0, -0.875),
    0.125 = math:fmod(8.125, -4),
    {'EXIT',{badarith,_}} = (catch math:fmod(5.0, 0.0)),

    %% Only for coverage (of beam_type.erl).
    {'EXIT',{undef,_}} = (catch math:fnurfla(0)),
    {'EXIT',{undef,_}} = (catch math:fnurfla(0, 0)),
    {'EXIT',{badarg,_}} = (catch float(kalle)),
    {'EXIT',{badarith,_}} = (catch name/1),
    ok.

mixed_float_and_int(Config) when is_list(Config) ->
    129.0 = pc(77, 23, 5),

    {'EXIT',{badarith,_}} = catch mixed_1(id({a,b,c})),
    {'EXIT',{{badarg,1/42},_}} = catch mixed_1(id(42)),

    ok.

pc(Cov, NotCov, X) ->
    round(Cov/(Cov+NotCov)*100) + 42 + 2.0*X.

mixed_1(V) ->
    {is_tuple(V) orelse 1 / V,
     1 / V andalso true}.

subtract_number_type(Config) when is_list(Config) ->
    120 = fact(5).

fact(N) ->
    fact(N, 1).

fact(0, P) -> P;
fact(1, P) -> P;
fact(N, P) -> fact(N-1, P*N).

float_followed_by_guard(Config) when is_list(Config) ->
    true = ffbg_1(5, 1),
    false = ffbg_1(1, 5),
    ok.

ffbg_1(A, B0) ->
    %% This is a non-guard block followed by a *guard block* that starts with a
    %% floating point operation, and the compiler erroneously assumed that it
    %% was safe to skip fcheckerror because the next block started with a float
    %% op.
    B = id(B0) / 1.0,
    if
        A - B > 0.0 -> true;
        A - B =< 0.0 -> false
    end.

%% ERL-1178: fconv instructions didn't inherit line numbers from their
%% respective BIF calls.
fconv_line_numbers(Config) when is_list(Config) ->
    fconv_line_numbers_1(id(gurka)),
    ok.

fconv_line_numbers_1(A) ->
    %% The ?LINE macro must be on the same line as the division.
    {'EXIT',{badarith, Stacktrace}} = (catch 10 / A), Line = ?LINE,
    true = lists:any(fun({?MODULE,?FUNCTION_NAME,1,[{file,_},{line,L}]}) ->
                             L =:= Line;
                        (_) ->
                             false
                     end, Stacktrace).

%% ERL-1471: compiler generated invalid 'fclearerror' / 'fcheckerror'
%% sequences.
exception_signals(Config) when is_list(Config) ->
    2.0 = exception_signals_1(id(25), id(true), []),
    2.0 = exception_signals_1(id(25), id(false), []),
    2.0 = exception_signals_1(id(25.0), id(true), []),
    2.0 = exception_signals_1(id(25.0), id(false), []),
    ok.

exception_signals_1(Width, Value, _Opts) ->
    Height = Width / 25.0,
    _Middle = case Value of
                  true -> Width / 2.0;
                  false -> 0
              end,
    _More = Height + 1.

id(I) -> I.

