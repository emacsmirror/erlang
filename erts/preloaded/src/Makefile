# 
# %CopyrightBegin%
#
# SPDX-License-Identifier: Apache-2.0
#
# Copyright Ericsson AB 2008-2025. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# %CopyrightEnd%
# 
# Note, this makefile is not called during normal build process, it should
# be used when the preloaded modules actually are to be updated (i.e. the 
# beam files are to be recompiled, which is normally not done). 
# The beam files are placed in the current directory and should be copied 
# to the ../ebin directory by using the copy target.

include $(ERL_TOP)/make/target.mk
EBIN=../../ebin
include $(ERL_TOP)/make/$(TARGET)/otp.mk

STATIC_EBIN=../ebin

include $(ERL_TOP)/erts/vsn.mk
include $(ERL_TOP)/lib/kernel/vsn.mk

PRE_LOADED_ERL_ESOCK_MODULES = \
	socket_registry \
	prim_socket \
	prim_net

PRE_LOADED_ERL_MODULES = \
	erl_prim_loader \
	init \
	prim_buffer \
	prim_file \
	prim_inet \
	$(PRE_LOADED_ERL_ESOCK_MODULES) \
	zlib \
	prim_zip \
	erl_init \
	erts_code_purger \
	erlang \
	erts_internal \
	erl_tracer \
	erts_literal_area_collector \
	erts_trace_cleaner \
	erts_dirty_process_signal_handler \
	atomics \
	counters \
        persistent_term

PRE_LOADED_BEAM_MODULES = \
	prim_eval

PRE_LOADED_MODULES = $(PRE_LOADED_ERL_MODULES) $(PRE_LOADED_BEAM_MODULES)

RELSYSDIR = $(RELEASE_PATH)/lib/erts-$(VSN)
# not $(RELEASE_PATH)/erts-$(VSN)/preloaded

ERL_FILES= $(PRE_LOADED_ERL_MODULES:%=%.erl)
BEAM_FILES= $(PRE_LOADED_BEAM_MODULES:%=%.S)
STUBS_FILES= $(PRE_LOADED_BEAM_MODULES:%=%.erl)

TARGET_FILES = $(PRE_LOADED_MODULES:%=$(EBIN)/%.$(EMULATOR)) \
    $(APP_TARGET)
STATIC_TARGET_FILES = $(PRE_LOADED_MODULES:%=$(STATIC_EBIN)/%.$(EMULATOR))

APP_FILE= erts.app
APP_SRC= $(APP_FILE).src
APP_TARGET= $(EBIN)/$(APP_FILE)
APP_ESOCK_MODS= prim_net, prim_socket, socket_registry,


KERNEL_SRC=$(ERL_TOP)/lib/kernel/src
KERNEL_INCLUDE=$(ERL_TOP)/lib/kernel/include
STDLIB_INCLUDE=$(ERL_TOP)/lib/stdlib/include

ERL_COMPILE_FLAGS += -I$(KERNEL_SRC) -I$(KERNEL_INCLUDE)

ifeq ($(ERL_DETERMINISTIC),yes)
	ERL_COMPILE_FLAGS += +deterministic
endif

DIA_PLT      = erts-preloaded.plt
DIA_ANALYSIS = $(basename $(DIA_PLT)).dialyzer_analysis
ifeq ($(DIAW_EH),true)
DIA_WARNINGS += -Werror_handling
endif
ifeq ($(DIAW_US),true)
DIA_WARNINGS += -Wunderspecs
endif
ifeq ($(DIAW_UR),true)
DIA_WARNINGS += -Wunmatched_returns
endif

debug opt: $(TARGET_FILES)

$(EBIN):
	$(V_at)mkdir "$@"

$(TARGET_FILES): | $(EBIN)

clean:
	rm -rf $(EBIN)

copy:
	cp $(EBIN)/*.beam $(STATIC_EBIN)
	$(V_ERL) -noshell -eval "[begin {ok, Bin} = file:read_file(File), {ok, {_, Stripped}} = beam_lib:strip(Bin), ok = file:write_file(File, zlib:gunzip(Stripped)) end || File <- filelib:wildcard(\"$(STATIC_EBIN)/*.beam\")]." -s init stop

$(APP_TARGET): $(APP_SRC) $(ERL_TOP)/erts/vsn.mk
	$(vsn_verbose)sed -e 's;%VSN%;$(VSN);' -e 's;%ESOCK_MODS%;$(APP_ESOCK_MODS);' $< > $@

#
# Combine a BEAM assembly script file a stub Erlang file into a BEAM file.
# See add_abstract_chunk script.
#

prim_eval.abstr: prim_eval.erl
	$(V_ERLC) $(ERL_COMPILE_FLAGS) -o$(dir $@) +dabstr $<

$(EBIN)/prim_eval.beam: prim_eval.S prim_eval.abstr
	$(gen_verbose)
	$(V_at)$(ERLC) $(ERL_COMPILE_FLAGS) -o$(dir $@) $<
	$(V_at)$(ESCRIPT) add_abstract_code $@ prim_eval.abstr || (rm $@; exit 1)


include $(ERL_TOP)/make/otp_release_targets.mk

release_spec: $(TARGET_FILES)
	$(INSTALL_DIR) "$(RELSYSDIR)/src"
	$(INSTALL_DATA) $(ERL_FILES) $(BEAM_FILES) $(STUBS_FILES) "$(RELSYSDIR)/src"
	$(INSTALL_DIR) "$(RELSYSDIR)/ebin"
	$(INSTALL_DATA) $(TARGET_FILES) "$(RELSYSDIR)/ebin"

release_docs_spec:


list_preloaded:
	@echo $(PRE_LOADED_MODULES)

dclean:
	rm -f $(DIA_PLT)
	rm -f $(DIA_ANALYSIS)

dialyzer_plt: $(DIA_PLT)

$(DIA_PLT): $(ERL_FILES)
	@echo "Building ($(basename $(DIA_PLT))) plt file"
	@dialyzer --build_plt \
                  --output_plt $@ \
                  -r ../ebin \
                     ../../../lib/kernel/ebin \
                     ../../../lib/stdlib/ebin \
                     ../../../lib/crypto/ebin \
                     ../../../lib/compiler/ebin \
                  --output $(DIA_ANALYSIS) \
                  --verbose

dialyzer: $(DIA_PLT)
	@echo "Running dialyzer on $(basename $(DIA_PLT))"
	@dialyzer --plt $< \
                  ../ebin \
                  $(DIA_WARNINGS) \
                  --verbose
DEP_REL_TOP=../../../lib/
include $(ERL_TOP)/make/dep.mk