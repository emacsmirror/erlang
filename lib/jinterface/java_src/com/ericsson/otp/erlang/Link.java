/*
 * %CopyrightBegin%
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright Ericsson AB 2000-2025. All Rights Reserved.
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
package com.ericsson.otp.erlang;

// package scope
class Link {
    private final OtpErlangPid local;
    private final OtpErlangPid remote;
    private long unlinking = 0;
    private int hashCodeValue = 0;

    public Link(final OtpErlangPid local, final OtpErlangPid remote) {
        this.local = local;
        this.remote = remote;
        this.unlinking = 0;
        
    }

    public OtpErlangPid local() {
        return local;
    }

    public OtpErlangPid remote() {
        return remote;
    }

    public boolean contains(final OtpErlangPid pid) {
        return local.equals(pid) || remote.equals(pid);
    }

    public boolean equals(final OtpErlangPid alocal, final OtpErlangPid aremote) {
        return local.equals(alocal) && remote.equals(aremote)
                || local.equals(aremote) && remote.equals(alocal);
    }

    public long getUnlinking() {
        return this.unlinking;
    }

    public void setUnlinking(long unlink_id) {
        this.unlinking = unlink_id;
    }

    @Override
    public int hashCode() {
        if (hashCodeValue == 0) {
            final OtpErlangObject.Hash hash = new OtpErlangObject.Hash(5);
            hash.combine(local.hashCode() + remote.hashCode());
            hashCodeValue = hash.valueOf();
        }
        return hashCodeValue;
    }
}
