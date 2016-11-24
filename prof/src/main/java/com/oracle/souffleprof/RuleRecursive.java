/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

package com.oracle.souffleprof;

import java.io.Serializable;

public class RuleRecursive extends Rule implements Serializable {

    private static final long serialVersionUID = 5362512692249185464L;
    private int version;

    public RuleRecursive(String name, int version, String id)  {
        super(name, id);
        this.version = version;
    }

    public int getVersion() {
        return version;
    }

    @Override
    public String toString() {
        StringBuilder result = new StringBuilder();
        result.append("{"+this.name+","+this.version+":");
        result.append(","+this.runtime+","+this.num_tuples+"}");
        return result.toString();
    }
}