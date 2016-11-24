/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

package com.oracle.souffleprof;

import java.util.Comparator;

/**
 * Compare operation for profile data for sorting data according to a key
 */
public enum DataComparator implements Comparator<Object[]> {
    TIME {
        public int compare(Object[] a, Object[] b) {
        	return Double.compare((Double)b[0], (Double)a[0]);
        }},
    NR_T {
        public int compare(Object[] a, Object[] b) {
        	return Double.compare((Double)b[1], (Double)a[1]);
        }},
    R_T {
        public int compare(Object[] a, Object[] b) {
        	return Double.compare((Double)b[2], (Double)a[2]);
        }},     
    C_T {
        public int compare(Object[] a, Object[] b) {
        	return Double.compare((Double)b[3], (Double)a[3]);
        }},
    TUP {
        public int compare(Object[] a, Object[] b) {
        	return Long.compare((Long)b[4], (Long)a[4]);
        }},
    NAME {
        public int compare(Object[] a, Object[] b) {
        	String aStr = (String)a[5];
        	String bStr = (String)b[5];
        	return Integer.valueOf(aStr.compareTo(bStr));
        }},
    ID {
        public int compare(Object[] a, Object[] b) {
        	String aStr = (String)a[6];
        	String bStr = (String)b[6];
        	return Integer.valueOf(aStr.compareTo(bStr));
        }},
    PER {
        public int compare(Object[] a, Object[] b) {
        	return Double.compare((Double)b[8], (Double)a[8]);
        }};

        public static Comparator<Object[]> decending(final Comparator<Object[]> other) {
            return new Comparator<Object[]>() {
                public int compare(Object[] o1, Object[] o2) {
                    return -1 * other.compare(o1, o2);
                }
            };
        }

        public static Comparator<Object[]> getComparator(final int sortDir, final DataComparator... multipleOptions) {
            return new Comparator<Object[]>() {
                public int compare(Object[] o1, Object[] o2) {
                    for (DataComparator option : multipleOptions) {
                        int result = option.compare(o1, o2);
                        if (result != 0) {
                            return sortDir * result;
                        }
                    }
                    return 0;
                }
            };
        }
}
