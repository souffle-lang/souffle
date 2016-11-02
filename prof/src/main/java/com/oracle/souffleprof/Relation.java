/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

package com.oracle.souffleprof;

import java.io.Serializable;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

public class Relation implements Serializable {

    private static final long serialVersionUID = 2674759421227104137L;
    private String name;
    private double runtime = 0;
    private long prev_num_tuples = 0;
    private long num_tuples = 0;
    private String id;
    private String locator;
    private int rul_id = 0;
    private int rec_id = 0;

    private List<Iteration> iterations;
    private Map<String, Rule> ruleMap;

    private boolean ready = true;

    public Relation(String name, String id) {
        this.name = name;
        ruleMap = new HashMap<String, Rule>();
        iterations = new ArrayList<Iteration>();
        this.id = id;
    }

    public String createID() {
        this.rul_id++;
        return "N" + this.id.substring(1) + "." + rul_id;
    }

    public String createRecID(String name) {
        for (Iteration iter : iterations) {
            for (Rule rul : iter.getRul_rec().values()) {
                if (rul.getName().equals(name)) {
                    return rul.getId();
                }
            }
        }
        this.rec_id++;
        return "C" + this.id.substring(1) + "." + this.rec_id;
    }

    public double getNonRecTime() {
        return this.runtime;
    }

    public double getRecTime() {
        double result = 0;
        for (Iteration iter : iterations) {
            result += iter.getRuntime();
        }
        return result;
    }

    public double getCopyTime() {
        double result = 0;
        for (Iteration iter : iterations) {
            result += iter.getCopy_time();
        }
        return result;
    }

    public long getNum_tuplesRel() {
        Long result = 0L;
        for (Iteration iter : iterations) {
            result += iter.getNum_tuples();
        }
        return this.num_tuples + result;
    }

    public long getNum_tuplesRul() {
        Long result = 0L;
        for (Rule rul : ruleMap.values()) {
            result += rul.getNum_tuples();
        }
        for (Iteration iter : iterations) {
            for (Rule rul : iter.getRul_rec().values()) {
                result += rul.getNum_tuples();
            }
        }
        return result;
    }

    public Long getTotNum_tuples() {
        return getNum_tuplesRel();
    }

    public Long getTotNumRec_tuples() {
        Long result = 0L;
        for (Iteration iter : iterations) {
            for (Rule rul : iter.getRul_rec().values()) {
                result += rul.getNum_tuples();
            }
        }
        return result;
    }

    public void setRuntime(double runtime) {
        this.runtime = runtime;
    }

    public void setNum_tuples(long num_tuples) {
        this.num_tuples = num_tuples;
    }

    @Override
    public String toString() {
        StringBuilder result = new StringBuilder();
        result.append("{\n" + name + ":" + runtime + ";" + num_tuples
                + "\n\nonRecRules:\n");
        for (Rule rul : ruleMap.values()) {
            result.append(rul.toString());
        }
        result.append("\n\niterations:\n");
        result.append(iterations.toString());
        result.append("\n}");
        return result.toString();
    }

    public String getName() {
        return this.name;
    }

    /**
     * @return the ruleMap
     */
    public Map<String, Rule> getRuleMap() {
        return this.ruleMap;
    }

    public List<Rule> getRuleRecList() {
        List<Rule> temp = new ArrayList<Rule>();
        for (Iteration iter : iterations) {
            for (Rule rul : iter.getRul_rec().values()) {
                temp.add(rul);
            }
        }
        return temp;
    }

    public List<Iteration> getIterations() {
        return iterations;
    }

    public String getId() {
        return id;
    }

    public String getLocator() {
        return locator;
    }

    public void setLocator(String locator) {
        this.locator = locator;
    }

	public boolean isReady() {
		return ready;
	}

	public void setReady(boolean ready) {
		this.ready = ready;
	}

	public long getPrev_num_tuples() {
		return prev_num_tuples;
	}

	public void setPrev_num_tuples(long prev_num_tuples) {
		this.prev_num_tuples = prev_num_tuples;
	}
}
