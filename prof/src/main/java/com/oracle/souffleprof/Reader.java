/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

package com.oracle.souffleprof;

import java.io.BufferedReader;
import java.io.BufferedWriter;
import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.FileReader;
import java.io.FileWriter;
import java.io.IOException;
import java.io.ObjectOutputStream;
import java.io.RandomAccessFile;
import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * Reads the profile information from a file 
 * and builds the data model for the profile information.
 */
public class Reader {

    public ProgramRun run;
    private File file;
    private static long filepointer;
    private boolean loaded = false;
    private boolean online;
    RunnableThread R1;
    String mes;

    public Reader(String arg, ProgramRun run, boolean vFlag, boolean online) {
        this.file = new File(arg);
        this.run = run;
        this.online = online;
        relation_map = new HashMap<String, Relation>();
        runtime = -1.0;
    }

    public void readFile() {

        try {

            if (online) {
                RandomAccessFile r = new RandomAccessFile(file, "r");
                String str = null;
                filepointer = file.length();
                while ((str = r.readLine()) != null) {

                    if (!str.isEmpty() && str.charAt(0) == '@') {
                        if (str.equals("@start-debug")) {
                            continue;
                        }
                        String[] part;
                        if (str.contains("\'") || str.contains("\"")) {
                            part = replace(str);
                        } else {
                            part = str.substring(1).split("\\s*;\\s*");
                        }
                        mes = str;
                        process(part);
                    }
                }
                r.seek(r.getFilePointer());
                r.close();
                this.loaded = true;
                R1 = new RunnableThread(file, filepointer, run);
                new Thread(R1).start();

            } else {
                BufferedReader br = new BufferedReader(new FileReader(file));
                String str = null;
                while ((str = br.readLine()) != null) {

                    if (!str.isEmpty() && str.charAt(0) == '@') {
                        if (str.equals("@start-debug")) {
                            continue;
                        }
                        String[] part;
                        if (str.contains("\'") || str.contains("\"")) {
                            part = replace(str);
                        } else {
                            part = str.substring(1).split("\\s*;\\s*");
                        }
                        mes = str;
                        process(part);
                    }
                }
                br.close();
                this.loaded = true;
            }

        } catch (FileNotFoundException e) {
            this.loaded = false;
        } catch (IOException e) {
            this.loaded = false;
            e.printStackTrace();
        } catch (Exception e) {
            this.loaded = false;
            System.err.println("Error: Invalid log file format:");
            System.err.println(mes);
        }

    }

    public void ser(String f_name) {
        try {

            FileOutputStream fileOut = new FileOutputStream(f_name);
            ObjectOutputStream out = new ObjectOutputStream(fileOut);
            out.writeObject(this.run);
            out.close();
            fileOut.close();
        } catch (Exception e) {
            e.printStackTrace();
        }

    }

    public void save(String f_name) {
        File theDir = new File("old_runs");
        if (!theDir.exists()) {
            theDir.mkdir();
        }

        try {
            File save_file;
            save_file = new File("old_runs/" + f_name);
            int i = 1;
            while (save_file.exists()) {

                save_file = new File("old_runs/" + f_name + i);
                i++;
            }
            save_file.createNewFile();

            FileWriter fw = new FileWriter(save_file.getAbsoluteFile());
            BufferedWriter bw = new BufferedWriter(fw);
            RandomAccessFile r = new RandomAccessFile(this.file, "r");

            bw.write(this.file.getAbsolutePath() + " ");
            Date dNow = new Date();
            SimpleDateFormat ft = new SimpleDateFormat(
                    "'created on' yyyy.MM.dd 'at' HH:mm:ss");
            bw.write(ft.format(dNow));
            bw.newLine();
            bw.newLine();

            String str = null;
            while ((str = r.readLine()) != null) {
                if (!str.isEmpty() && str.charAt(0) == '@') {
                    bw.write(str);
                    bw.newLine();
                }
            }
            r.close();
            bw.close();

        } catch (IOException e) {
            e.printStackTrace();
        }
    }

    private String[] replace(String str) {
        boolean ignore = false;
        String temp1 = "";
        if (str.contains("\'")) {

            for (int i = 0; i < str.length(); i++) {
                if (str.charAt(i) == '\'') {
                    ignore = !ignore;
                }
                if (ignore && str.charAt(i) == ';') {
                    temp1 += '\b';
                } else {
                    temp1 += str.charAt(i);
                }
            }
        } else {
            temp1 = str;
        }
        ignore = false;
        String temp2 = "";
        if (temp1.contains("\"")) {
            for (int i = 0; i < temp1.length(); i++) {
                if (temp1.charAt(i) == '\"') {
                    ignore = !ignore;
                }
                if (ignore && temp1.charAt(i) == ';') {
                    temp2 += '\t';
                } else {
                    temp2 += temp1.charAt(i);
                }
            }
        } else {
            temp2 = temp1;
        }
        String[] part = temp2.substring(1).split("\\s*;\\s*");
        for (int i = 0; i < part.length; i++) {
            part[i] = part[i].replace('\b', ';');
            part[i] = part[i].replace('\t', ';');
        }
        return part;
    }

    public void stopRead() {
        R1.kill();
    }

    public boolean isUpdated() {
        return R1.isUpdated();
    }

    public void setUpdated() {
        R1.setUpdated();
    }

    public boolean isLoaded() {
        return this.loaded;

    }

    /* refactor Raihaan's SouffleProf v2.1.8 */
    private Double runtime;
    private Map<String, Relation> relation_map;
    private int rel_id = 0;
    
    
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

				} else if (data[0].charAt(0) == 'n' && data[0].contains("relation")) {
					rel.setNum_tuples(Long.parseLong(data[3]));

				} else if (data[0].contains("rule")) {
//					rel.addRule(data);
					addRule(rel, data);
				}

			} else if (data[0].contains("recursive")) {
//				rel.addIteration(data);
				addIteration(rel, data);
			}
		}
		
		run.setRuntime(this.runtime);
		run.setRelation_map(this.relation_map);
	}
	
	/**
	 * @return
	 */
	private String createId() {
		this.rel_id++;
		return "R" + this.rel_id;
	}

	/**
	 * @param data = [x-rul; rel_name; version; loc; rul_name; val] 
	 * 				 [x-rel; rel_name; loc; val]
	 */
	public void addIteration(Relation rel, String[] data) {

		boolean ready = rel.isReady();
		List<Iteration> iterations = rel.getIterations();
		String locator = rel.getLocator();

		Iteration iter;
		if (ready || iterations.isEmpty()) {
			iter = new Iteration();
			iterations.add(iter);
			// ready = false;
			rel.setReady(false);
		} else {
			iter = iterations.get(iterations.size() - 1);
		}

		if (data[0].contains("rule")) {
			String temp = rel.createRecID(data[4]);
			iter.addRule(data, temp);

		} else if (data[0].charAt(0) == 't' && data[0].contains("relation")) {
			iter.setRuntime(Double.parseDouble(data[3]));
			iter.setLocator(data[2]);
			// locator = (String) data[2];
			rel.setLocator((String) data[2]);
		} else if (data[0].charAt(0) == 'n' && data[0].contains("relation")) {
			iter.setNum_tuples(Long.parseLong(data[3]));
		} else if (data[0].charAt(0) == 'c' && data[0].contains("relation")) {
			iter.setCopy_time(Double.parseDouble(data[3]));
			// ready = true;
			rel.setReady(true);
		}

	}
    
	/*
	 * Adds non-recursive rule to this relation.
	 */
	public void addRule(Relation rel, String[] data) {
		Map<String, Rule> ruleMap = rel.getRuleMap();
		long prev_num_tuples = rel.getPrev_num_tuples();

		Rule rul;
		if (!ruleMap.containsKey(data[3])) {
			rul = new Rule(data[3], rel.createID());
			ruleMap.put(data[3], rul);
		} else {
			rul = ruleMap.get(data[3]);
		}

		if (data[0].charAt(0) == 't') {
			// int len = data.length;
			rul.setRuntime(Double.parseDouble(data[4]));
			rul.setLocator(data[2]);
		} else if (data[0].charAt(0) == 'n') {
			assert rul != null;
			rul.setNum_tuples(Long.parseLong(data[4]) - prev_num_tuples);
			// this.prev_num_tuples = Long.parseLong(data[4]);
			rel.setPrev_num_tuples(Long.parseLong(data[4]));
		}

	}
}

class RunnableThread implements Runnable {

    private long filepointer;
    private long prev_filepointer;
    private String prev_line = "";
    private File file;
    private boolean running;
    private boolean changed = false; // if update occurs before display
    // acknowledgement
    private volatile boolean updated = false; // if display reflects updated
    // data model
    private ProgramRun run;
    private Reader reader;
    private String[] test;

    RunnableThread(File file, long fp, ProgramRun run) {
        this.file = file;
        this.filepointer = fp;
        this.prev_filepointer = fp;
        running = true;
        this.run = run;
    }

    RunnableThread(File file, long fp, Reader reader) {
    	this.file = file;
    	this.filepointer = fp;
    	this.prev_filepointer = fp;
    	running = true;
    	this.reader = reader;
    }
    
    @Override
    public void run() {
        try {
            while (running) {
                Thread.sleep(1000);
                long len = file.length();
                if (len < filepointer) {
                    // Log must have been deleted.
                    filepointer = len;
                } else if (len > filepointer) {
                    // File must have had something added to it!

                    if (updated) {
                        changed = true;
                    } else {
                        updated = true;
                        changed = false;
                    }
                    RandomAccessFile raf = new RandomAccessFile(file, "r");
                    if (filepointer != prev_filepointer) {
                        raf.seek(prev_filepointer);
                        if (raf.readLine().equals(prev_line)) {

                        }
                    }

                    raf.seek(filepointer);
                    String line = "";
                    int c;

                    while (true) {
                        line = "";
                        prev_filepointer = raf.getFilePointer();
                        while (true) {
                            if ((c = raf.read()) == -1) {
                                break;
                            } else if ((char) c == '\n') {
                                break;
                            } else {
                                line += (char) c;
                                continue;
                            }
                        }

                        if (line.isEmpty() && c == -1) {
                            filepointer = raf.getFilePointer();
                            raf.close();
                            break;
                        } else if (line.isEmpty() && (char) c == '\n') {
                            filepointer = prev_filepointer;
                            break;
                        } else if (!line.isEmpty() && (char) c != '\n') {
                            filepointer = prev_filepointer;
                            break;
                        }
                        if (!line.isEmpty() && line.charAt(0) == '@') {
                            String[] part;
                            if (line.contains("\'") || line.contains("\"")) {
                                part = replace(line);
                            } else {
                                part = line.substring(1).split("\\s*;\\s*");
                            }
                            test = part;
                            reader.process(part);
                        }

                    }

                }
            }

        } catch (Exception e) {
            e.printStackTrace();
            for (String s : test) {
                System.out.print(s + ", ");
            }
            System.out.print("\b\b  \n");
        }
    }

    public void kill() {
        this.running = false;
    }

    public boolean isUpdated() {
        return this.updated;
    }

    public void setUpdated() {
        if (changed) {
            this.updated = true;
            this.changed = false;
        } else {
            this.updated = false;
        }
    }

    private String[] replace(String str) {
        boolean ignore = false;
        String temp1 = "";
        if (str.contains("\'")) {

            for (int i = 0; i < str.length(); i++) {
                if (str.charAt(i) == '\'') {
                    ignore = !ignore;
                }
                if (ignore && str.charAt(i) == ';') {
                    temp1 += '\b';
                } else {
                    temp1 += str.charAt(i);
                }
            }
        } else {
            temp1 = str;
        }
        ignore = false;
        String temp2 = "";
        if (temp1.contains("\"")) {
            for (int i = 0; i < temp1.length(); i++) {
                if (temp1.charAt(i) == '\"') {
                    ignore = !ignore;
                }
                if (ignore && temp1.charAt(i) == ';') {
                    temp2 += '\t';
                } else {
                    temp2 += temp1.charAt(i);
                }
            }
        } else {
            temp2 = temp1;
        }
        String[] part = temp2.substring(1).split("\\s*;\\s*");
        for (int i = 0; i < part.length; i++) {
            part[i] = part[i].replace('\b', ';');
            part[i] = part[i].replace('\t', ';');
        }
        return part;
    }
}
