/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

package com.oracle.souffleprof;

import java.io.Serializable;
import java.util.HashMap;
import java.util.Map;

/***
 * Profile Data Model
 * 
 * Represents recursive profile data 
 */

public class Iteration implements Serializable {

	private static final long serialVersionUID = 4513587123648922999L;
	private double runtime = 0;
	private long num_tuples = 0;
	private double copy_time = 0;
	private String locator = "";
	private long prev_num_tuples = 0;

	private Map<String, Rule> rul_rec_map;

	public Iteration() {
		this.rul_rec_map = new HashMap<String, Rule>();
	}

	public void addRule(String[] data, String rec_id) {
		String strTemp = data[4] + data[3] + data[2];

		if (data[0].charAt(0) == 't') {
			boolean exists = false;

			for (String name : rul_rec_map.keySet()) {
				if (name.equals(strTemp)) {
					Rule rul_rec = rul_rec_map.get(strTemp);
					rul_rec.setRuntime(Double.parseDouble(data[5])
							+ rul_rec.getRuntime());
					exists = true;
				}
			}
			if (!exists) {
				Rule rul_rec = new Rule(data[4], 
						Integer.parseInt(data[2]), rec_id);
				rul_rec.setRuntime(Double.parseDouble(data[5]));
				rul_rec.setLocator(data[3]);
				rul_rec_map.put(strTemp, rul_rec);
			}

		} else if (data[0].charAt(0) == 'n') {
			Rule rul_rec = rul_rec_map.get(strTemp);
			assert rul_rec != null : "missing t tag";
			rul_rec.setNum_tuples(Long.parseLong(data[5]) - prev_num_tuples);
			this.prev_num_tuples = Long.parseLong(data[5]);
			rul_rec_map.put(strTemp, rul_rec);
		}
	}

	public Map<String, Rule> getRul_rec() {
		return rul_rec_map;
	}

	@Override
	public String toString() {

		StringBuilder res = new StringBuilder();
		res.append("" + runtime + "," + num_tuples + "," + copy_time + ",");
		res.append(" recRule:");
		for (Rule rul : rul_rec_map.values()) {
			res.append(rul.toString());
		}
		res.append("\n");
		return res.toString();
	}

	public double getRuntime() {
		return runtime;
	}

	public void setRuntime(double runtime) {
		this.runtime = runtime;
	}

	public long getNum_tuples() {
		return num_tuples;
	}

	public void setNum_tuples(long num_tuples) {
		this.num_tuples = num_tuples;
	}

	public double getCopy_time() {
		return copy_time;
	}

	public void setCopy_time(double copy_time) {
		this.copy_time = copy_time;
	}

	public String getLocator() {
		return locator;
	}

	public void setLocator(String locator) {
		this.locator = locator;
	}
}
