/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

package com.oracle.souffleprof;

import java.io.Serializable;
import java.text.DecimalFormat;
import java.util.HashMap;
import java.util.Map;
import java.util.concurrent.TimeUnit;

/**
 * ProgramRun
 * 
 * Profile data model to represent a run of a Datalog program.
 * 
 */
public class ProgramRun implements Serializable{

    /**
     * 
     */
    private static final long serialVersionUID = -4236231035756069616L;
    private Map<String, Relation> relation_map;
    private int rel_id = 0;
    private Double runtime;
    private Double tot_rec_tup = 0.0;
    private Double tot_copy_time = 0.0;


    public ProgramRun() {
        relation_map = new HashMap<String, Relation>();
        runtime = -1.0;
    }

    public void update() {
        tot_rec_tup = getTotNumRecTuples().doubleValue();
        tot_copy_time = getTotCopyTime();
    }

    /**
     * Inserts profile data from part into data model.
     * 
     * @param data
     */
    public void process(String[] data) {

        if (data[0].equals("runtime")) {
            this.runtime = Double.parseDouble(data[1]);

        } else {

            Relation rel;
            if (!relation_map.containsKey(data[1])) {
                rel = new Relation(data[1], createId());
                relation_map.put(data[1], rel);
            } else {
                rel = relation_map.get(data[1]);
            }

            if (data[0].contains("nonrecursive")) {

                if (data[0].charAt(0) == 't' && data[0].contains("relation")) {
                    rel.setRuntime(Double.parseDouble(data[3]));
                    rel.setLocator(data[2]);

                } else if (data[0].charAt(0) == 'n'
                        && data[0].contains("relation")) {
                    rel.setNum_tuples(Long.parseLong(data[3]));

                } else if (data[0].contains("rule")) {
                    rel.addRule(data);
                }

            } else if (data[0].contains("recursive")) {
                rel.addIteration(data);
            }
        }

    }

    /**
     * @return
     */
    private String createId() {
        this.rel_id++;
        return "R" + this.rel_id;
    }

    @Override
    public String toString() {
        StringBuilder result = new StringBuilder();
        result.append("ProgramRun:" + runtime + "\nRelations:\n");
        for (Relation r : relation_map.values()) {
            result.append(r.toString() + "\n");
        }
        return result.toString();
    }

    public Map<String, Relation> getRelation_map() {
        return relation_map;
    }

    public String getRuntime() {
        if (runtime == -1.0) {
            return "--";
        }
        return formatTime(runtime)+"";
    }

    public Long getTotNumTuples() {
        Long result = 0L;
        for (Relation r : relation_map.values()) {
            result += r.getTotNum_tuples();
        }
        return result;
    }

    public Long getTotNumRecTuples() {
        Long result = 0L;
        for (Relation r : relation_map.values()) {
            result += r.getTotNumRec_tuples();
        }
        return result;
    }

    public double getTotCopyTime() {
        double result = 0;
        for (Relation r : relation_map.values()) {
            result += r.getCopyTime();
        }
        return result;
    }

    public double getTotTime() {
        double result = 0;
        for (Relation r : relation_map.values()) {
            result += r.getRecTime() + r.getNonRecTime() + r.getCopyTime();
        }
        return  result;
    }

    /*
        rel table :
        ROW[0] = TOT_T
        ROW[1] = NREC_T
        ROW[2] = REC_T
        ROW[3] = COPY_T
        ROW[4] = TUPLES
        ROW[5] = REL NAME
        ROW[6] = ID
        ROW[7] = SRC
        ROW[8] = PERFOR

     */
    public Object[][] getRelTable() {
        Object[][] table = new Object[relation_map.size()][];
        int i = 0;
        for (Relation r : relation_map.values()) {
            Object[] row = new Object[9];
            row[0] = r.getNonRecTime() + r.getRecTime() + r.getCopyTime();
            row[1] = r.getNonRecTime();
            row[2] = r.getRecTime();
            row[3] = r.getCopyTime();
            row[4] = r.getNum_tuplesRel();
            row[5] = r.getName();
            row[6] = r.getId();
            row[7] = r.getLocator();
            if ((Double)row[0] != 0.0) {
                row[8] = (Double)((Long)row[4]/(Double)row[0]);
            } else {
                row[8] = (Double)((Long)row[4]/1.0);
            }
            table[i++] = row;
        }
        return table;
    }

    /*
        rul table :
        ROW[0] = TOT_T
        ROW[1] = NREC_T
        ROW[2] = REC_T
        ROW[3] = COPY_T
        ROW[4] = TUPLES
        ROW[5] = RUL NAME
        ROW[6] = ID
        ROW[7] = SRC
        ROW[8] = PERFOR
        ROW[9] = VER
        ROW[10]= REL_NAME
     */
    public Object[][] getRulTableGui() {
        Map<String, Object[]> rule_map = new HashMap<String, Object[]>();

        for (Relation rel : relation_map.values()) {
            for (Rule rul : rel.getRuleMap().values()) {
                Object[] temp = new Object[11];
                temp[1] = rul.getRuntime();
                temp[2] = 0.0;
                temp[3] = 0.0;
                temp[4] = rul.getNum_tuples();
                temp[5] = rul.getName();
                temp[6] = rul.getId();
                temp[7] = rul.getLocator();
                temp[9] = 0;
                temp[10] = rel.getName();
                rule_map.put(rul.getName(), temp);
            }

            for (Iteration iter : rel.getIterations()) {
                Object[] temp;
                for (RuleRecursive rul : iter.getRul_rec().values()) {

                    if (rule_map.containsKey(rul.getName())) {
                        temp = rule_map.get(rul.getName());
                        temp[2] = (Double) temp[2] + (Double) rul.getRuntime();
                        temp[4] = (Long) temp[4] + rul.getNum_tuples();
                    } else {
                        temp = new Object[11];
                        temp[1] = 0.0;
                        temp[2] = (Double) rul.getRuntime();
                        temp[3] = 0.0;
                        temp[4] = (Long) rul.getNum_tuples();
                        temp[6] = rul.getId();
                        temp[5] = rul.getName();
                        temp[9] = rul.getVersion();
                        temp[10] = rel.getName();

                    }
                    temp[0] = rul.getRuntime();
                    rule_map.put(rul.getName(), temp);
                }
            }
            for (Object[] t : rule_map.values()) {
                if (((String) t[6]).charAt(0) == 'C') {
                    if (getTotNumRecTuples() != 0) {
                        t[3] = (getTotCopyTime() / getTotNumRecTuples()) * (Long) t[4];
                    } else {
                        t[3] = 0.0;
                    }
                }
                t[0] = (Double) t[1] + (Double) t[2] + (Double) t[3];

                if ((Double)t[0] != 0.0) {
                    t[8] = (Double)((Long)t[4]/(Double)t[0]);
                } else {
                    t[8] = (Double)((Long)t[4]/1.0);
                }
            }
        }
        Object[][] table = new Object[rule_map.size()][];
        int i = 0;
        for (Object[] row : rule_map.values()) {
            table[i++] = row;
        }
        return table;
    }

    /*
        rul table :
        ROW[0] = TOT_T
        ROW[1] = NREC_T
        ROW[2] = REC_T
        ROW[3] = COPY_T
        ROW[4] = TUPLES
        ROW[5] = RUL NAME
        ROW[6] = ID
        ROW[7] = REL_NAME
        ROW[8] = VER
        ROW[9] = PERFOR
        ROW[10]= SRC
     */
    public Object[][] getRulTable() {
        Map<String, Object[]> rule_map = new HashMap<String, Object[]>();

        for (Relation rel : relation_map.values()) {
            for (Rule rul : rel.getRuleMap().values()) {
                Object[] temp = new Object[11];
                temp[1] = rul.getRuntime();
                temp[2] = 0.0;
                temp[3] = 0.0;
                temp[4] = rul.getNum_tuples();
                temp[5] = rul.getName();
                temp[6] = rul.getId();
                temp[7] = rel.getName();
                temp[8] = 0;
                temp[10] = rul.getLocator();
                rule_map.put(rul.getName(), temp);
            }

            for (Iteration iter : rel.getIterations()) {
                Object[] temp;
                for (RuleRecursive rul : iter.getRul_rec().values()) {

                    if (rule_map.containsKey(rul.getName())) {
                        temp = rule_map.get(rul.getName());
                        temp[2] = (Double) temp[2] + (Double) rul.getRuntime();
                        temp[4] = (Long) temp[4] + rul.getNum_tuples();
                    } else {
                        temp = new Object[11];
                        temp[1] = 0.0;
                        temp[2] = (Double) rul.getRuntime();
                        temp[3] = 0.0;
                        temp[4] = (Long) rul.getNum_tuples();
                        temp[6] = rul.getId();
                        temp[5] = rul.getName();
                        temp[7] = rel.getName();
                        temp[8] = rul.getVersion();
                    }
                    temp[0] = rul.getRuntime();
                    rule_map.put(rul.getName(), temp);
                }
            }

            for (Object[] t : rule_map.values()) {
                if (((String) t[6]).charAt(0) == 'C') {
                    if (getTotNumRecTuples() != 0) {
                        t[3] = (getTotCopyTime() / getTotNumRecTuples()) * (Long) t[4];
                    } else {
                        t[3] = 0.0;
                    }
                }
                t[0] = (Double) t[1] + (Double) t[2] + (Double) t[3];

                if ((Double)t[0] != 0.0) {
                    t[9] = (Double)((Long)t[4]/(Double)t[0]);
                } else {
                    t[9] = (Double)((Long)t[4]/1.0);
                }
            }
        }
        Object[][] table = new Object[rule_map.size()][];
        int i = 0;
        for (Object[] row : rule_map.values()) {
            table[i++] = row;
        }
        return table;
    }

    /*
     ver table :
      ROW[0] = TOT_T
     ROW[1] = NREC_T
     ROW[2] = REC_T
     ROW[3] = COPY_T
     ROW[4] = TUPLES
     ROW[5] = RUL NAME
     ROW[6] = ID
     ROW[7] = SRC
     ROW[8] = PERFOR
     ROW[9] = VER
     ROW[10]= REL_NAME
     */
    public Object[][] getVersionsGui(String strRel, String strRul) {
        Map<String, Object[]> rule_map = new HashMap<String, Object[]>();

        for (Relation rel : relation_map.values()) {
            if (rel.getId().equals(strRel)) {

                for (Iteration iter : rel.getIterations()) {
                    Object[] temp;
                    for (RuleRecursive rul : iter.getRul_rec().values()) {

                        if (rul.getId().equals(strRul)) {
                            String strTemp = rul.getName() + rul.getLocator()
                                    + rul.getVersion();
                            if (rule_map.containsKey(strTemp)) {
                                temp = rule_map.get(strTemp);
                                temp[2] = (Double) temp[2]
                                        + (Double) rul.getRuntime();
                                temp[4] = (Long) temp[4] + rul.getNum_tuples();

                            } else {
                                temp = new Object[11];
                                temp[1] = 0.0;
                                temp[2] = (Double) rul.getRuntime();
                                temp[4] = (Long) rul.getNum_tuples();
                                temp[5] = rul.getName();
                                temp[6] = rul.getId();
                                temp[7] = rul.getLocator();
                                temp[8] = rul.getVersion();
                                temp[10] = rel.getName(); 
                            }
                            temp[0] = rul.getRuntime();
                            rule_map.put(strTemp, temp);
                        }
                    }
                }
                for (Object[] t : rule_map.values()) {
                    if (tot_rec_tup != 0) {
                        Double rec_tup = (Double) t[4];
                        t[3] = (tot_copy_time / tot_rec_tup) * rec_tup;
                    } else {
                        t[3] = 0.0;
                    }
                    t[0] = (Double) t[1] + (Double) t[2] + (Double) t[3];
                    if ((Double) t[0] != 0.0) {
                        t[9] = (Double) ((Long) t[4] / (Double) t[0]);
                    } else {
                        t[9] = (Double) ((Long) t[4] / 1.0);
                    }
                }
                break;
            }

        }
        Object[][] table = new Object[rule_map.size()][];
        int i = 0;
        for (Object[] row : rule_map.values()) {
            table[i++] = row;
        }
        return table;
    }

    /*
        rul table :
        ROW[0] = TOT_T
        ROW[1] = NREC_T
        ROW[2] = REC_T
        ROW[3] = COPY_T
        ROW[4] = TUPLES
        ROW[5] = RUL NAME
        ROW[6] = ID
        ROW[7] = REL_NAME
        ROW[8] = VER
        ROW[9] = LOCATOR
     */
    public Object[][] getVersions(String strRel, String strRul) {
        Map<String, Object[]> rule_map = new HashMap<String, Object[]>();

        for (Relation rel : relation_map.values()) {
            if (rel.getId().equals(strRel)) {

                for (Iteration iter : rel.getIterations()) {
                    Object[] temp;
                    for (RuleRecursive rul : iter.getRul_rec().values()) {

                        if (rul.getId().equals(strRul)) {
                            String strTemp = rul.getName() + rul.getLocator()
                                    + rul.getVersion();
                            if (rule_map.containsKey(strTemp)) {
                                temp = rule_map.get(strTemp);
                                temp[2] = (Double) temp[2]
                                        + (Double) rul.getRuntime();
                                temp[4] = (Long) temp[4] + rul.getNum_tuples();

                            } else {
                                temp = new Object[10];
                                temp[1] = 0.0;
                                temp[2] = (Double) rul.getRuntime();
                                temp[4] = (Long) rul.getNum_tuples();
                                temp[5] = rul.getName();
                                temp[6] = rul.getId();
                                temp[7] = rel.getName();
                                temp[8] = rul.getVersion();
                                temp[9] = rul.getLocator();
                            }
                            temp[0] = rul.getRuntime();
                            rule_map.put(strTemp, temp);
                        }
                    }
                }
                for (Object[] t : rule_map.values()) {
                    if (getTotNumRecTuples() != 0) {
                        t[3] = (getTotCopyTime() / getTotNumRecTuples()) * (Long) t[4];
                    } else {
                        t[3] = 0.0;
                    }
                    t[0] = (Double) t[1] + (Double) t[2] + (Double) t[3];
                }
                break;
            }

        }
        Object[][] table = new Object[rule_map.size()][];
        int i = 0;
        for (Object[] row : rule_map.values()) {
            table[i++] = row;
        }
        return table;
    }



    public Relation getRelation(String name) {
        for (Relation rel : getRelation_map().values()) {
            if (rel.getName().equals(name)) {
                return rel;
            }
        }
        return null;
    }

    public String[][] formatTable(Object[][] table, int precision) {
        String[][] new_table = new String[table.length][table[0].length];
        for (int i = 0; i < table.length; i++) {
            for (int j = 0; j < table[0].length; j++) {
                if (table[i][j] instanceof Double) {
                    new_table[i][j] = formatTime((Double)table[i][j]);
                } else if (table[i][j] instanceof Long) {
                    new_table[i][j] = formatNum(precision, (Long)table[i][j]);
                } else if (table[i][j] != null) {
                    new_table[i][j] = table[i][j].toString();
                } else {
                    new_table[i][j] = "-";
                }
            }
        }
        return new_table;
    }

    public String formatNum(int precision, Object number) {
        Long amount;
        if (number != null && number instanceof Long) {
            amount = (Long) number;
        } else {
            return "0";
        }
        if (precision == -1) {
            return Long.toString(amount);
        }

        String result;
        DecimalFormat formatter;
        if (amount >= 1000000000) {
            amount = (amount + 5000000) / 10000000;
            result = amount + "B";
            result = result.substring(0, 1) + "." + result.substring(1);
        } else if (amount >= 100000000) {
            amount = (amount + 500000) / 1000000;
            result = amount + "M";
            formatter = new DecimalFormat("###,###");
        } else if (amount >= 1000000) {
            amount = (amount + 5000) / 10000;

            result = amount + "M";
            result = result.substring(0, result.length() - 3) + "."
                    + result.substring(result.length() - 3, result.length());
        } else {
            formatter = new DecimalFormat("###,###");
            result = formatter.format(amount);
        }

        return result;
    }

    public String formatTime(Object number) {
        Double time = null;
        if (number instanceof Double) {
            time = (Double) number;
        }
        if (time == null || time.isNaN()) {
            return "-";
        }

        long val;
        long sec = Math.round(time);
        if (sec >= 100) {
            val = TimeUnit.SECONDS.toMinutes(sec);
            if (val < 100) {
                if (val < 10) {
                    String temp = (double) ((double) (sec - (TimeUnit.MINUTES
                            .toSeconds(val))) / 60) * 100 + "";
                    return val + "." + temp.substring(0, 1) + "m";
                }
                return val + "m";
            }
            val = TimeUnit.MINUTES.toHours(val);
            if (val < 100) {
                return val + "h";
            }
            val = TimeUnit.HOURS.toDays(val);
            return val + "D";
        } else if (sec >= 10) {
            return sec + "";
        } else if (Double.compare(time, 1.0) >= 0) {
            DecimalFormat formatter = new DecimalFormat("0.00");
            return formatter.format(number);
        } else if (Double.compare(time, 0.001) >= 0) {
            DecimalFormat formatter = new DecimalFormat(".000");
            return formatter.format(number);
        }
        return ".000";
    }
}
