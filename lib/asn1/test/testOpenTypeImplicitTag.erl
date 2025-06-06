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
%%
-module(testOpenTypeImplicitTag).

-export([main/1]).

-include_lib("common_test/include/ct.hrl").


main(jer) ->
    roundtrip('Seq', {'Seq',<<"123">>,<<"456">>,12,<<"789">>}),
    roundtrip('Seq', {'Seq',<<"4711">>,asn1_NOVALUE,12,<<"1137">>}),
    ok;
main(_Rules) ->
    roundtrip('Seq', {'Seq',<<1,1,255>>,<<1,1,255>>,12,<<1,1,255>>}),
    roundtrip('Seq', {'Seq',<<1,1,255>>,asn1_NOVALUE,12,<<1,1,255>>}),
    ok.


roundtrip(T, V) ->
    asn1_test_lib:roundtrip('OpenTypeImplicitTag', T, V).
