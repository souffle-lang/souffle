/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

package com.oracle.souffleprof;

import java.io.Serializable;

public class Rule implements Serializable {

    private static final long serialVersionUID = -2518455587078964434L;
    protected String name;
    protected double runtime = 0;
    protected long num_tuples = 0;
    protected String id;
    protected String locator = "";
    
    private boolean isRecursive = false;
    private int version;

    public Rule(String name, String id) {
        this.name = name;
        this.id = id;
    }
    
    public Rule(String name, int version, String id)  {
        this.name = name;
        this.id = id;
        this.isRecursive = true;
        this.version = version;
    }

    public String getId() {
        return id;
    }

    public double getRuntime() {
        return runtime;
    }

    public long getNum_tuples() {
        return num_tuples;
    }

    public void setRuntime(double runtime) {
        this.runtime = runtime;
    }

    public void setNum_tuples(long num_tuples) {
        this.num_tuples = num_tuples;
    }

    public String getName() {
        return name;
    }

    public void setId(String id) {
        this.id = id;
    }

    public String getLocator() {
        return locator;
    }

    public void setLocator(String locator) {
        if (this.locator.isEmpty()) {
            this.locator = locator;
        } else {
            this.locator += " " + locator;
        }

    }

    public boolean isRecursive() {
		return isRecursive;
	}

	public void setRecursive(boolean isRecursive) {
		this.isRecursive = isRecursive;
	}

	public int getVersion() {
		return version;
	}

	public void setVersion(int version) {
		this.version = version;
	}

	@Override
    public String toString() {
        StringBuilder result = new StringBuilder();
        
        if (isRecursive) {
			result.append("{" + name + "," + version + ":");
        } else {
        	result.append("{" + name + ":");
        }
        
        result.append("[" + runtime + "," + num_tuples + "]");
        result.append("}");
        return result.toString();
    }

}
