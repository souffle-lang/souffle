/**
 *
 */
package com.oracle.souffleprof;

import java.io.*;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.List;

import com.google.gson.Gson;

/**
 * @author ramod
 *
 */
public class Tui {
	
	private OutputProcessor out;
    private boolean loaded;
    private String f_name;
    private Reader live_reader;
    private boolean alive = false;
    private int sort_col = 0;
    private int precision = -1;
    private Object[][] rel_table_state;
    private Object[][] rul_table_state;
    private int sortDir = 1;

    public Tui(String f_name, boolean live) {
    	out = new OutputProcessor();
        ProgramRun run = out.getProgramRun();
        Reader reader = new Reader(f_name, run, false, live);
        reader.readFile();
        if (live) {
            this.live_reader = reader;
        }
        this.loaded = reader.isLoaded();
        this.f_name = f_name;
        this.alive = live;
        rul_table_state = out.getRulTable();
        rel_table_state = out.getRelTable();
    }

    public void runCommand(String[] c) {
        if (!loaded) {
            System.out.println("Error: File cannot be loaded");
            return;
        }
        if (c[0].equals("top")) {
            top();
        } else if (c[0].equals("rel")) {
            if (c.length == 2) {
                relRul(c[1]);
            } else if (c.length == 1) {
                rel(c[0]);
            } else {
                System.out.println("Invalid parameters to rel command.");
                help();
            }
        } else if (c[0].equals("rul")) {
            if (c.length > 1) {
                if (c.length == 3 && c[1].equals("id")) {
                    System.out.print(String.format("%7s%2s%-25s\n\n", "ID", "", "NAME"));
                    id(c[2]);
                } else if (c.length == 2 && c[1].equals("id")) {
                    id("0");
                } else if (c.length == 2) {
                    verRul(c[1]);
                } else {
                    System.out.println("Invalid parameters to rul command.");
                    help();
                }
            } else {
                rul(c[0]);
            }
        } else if (c[0].equals("graph")) {
            if (c.length == 3 && !c[1].contains(".")) {
                iterRel(c[1], c[2]);
            } else if (c.length == 3 && c[1].charAt(0) == 'C') {
                iterRul(c[1], c[2]);
            } else if (c.length == 4 && c[1].equals("ver")
                    && c[2].charAt(0) == 'C') {
                verGraph(c[2], c[3]);
            } else {
                System.out.println("Invalid parameters to graph command.");
                help();
            }
        } else if (c[0].equals("help")) {
            help();
        } else {
            System.out.println("Unknown command. Please select from the following commands:"); 
            help();
        }
    }

    public void runGui() {
    	
    	Gson gson = new Gson();
    	
    	// Output overview
    	ProgramRun run = out.getProgramRun();
    	HashMap<String, String> overview = new HashMap<>();
    	overview.put("Runtime", run.getRuntime());
    	overview.put("TotTime", run.formatTime(run.getTotTime()));
    	overview.put("TotCopyTime", run.formatTime(run.getTotCopyTime()));
    	overview.put("TotNumTuples", run.formatNum(precision, run.getTotNumTuples()));
    	overview.put("TotNumRecTuples", run.formatNum(precision, run.getTotNumRecTuples()));
    	
    	String overviewJson = gson.toJson(overview);
    	File file = new File("/home/dennis/WebstormProjects/SouffleProfGui/json/overview.json");
    	try {
    		FileWriter fw = new FileWriter(file);
    		BufferedWriter bw = new BufferedWriter(fw);
    		bw.write(overviewJson);
    		bw.close();
    	} catch (IOException e) {
    		e.printStackTrace();
    	}
    	
    	// Output properties
    	HashMap<String, String> properties = new HashMap<>();
    	properties.put("LogFileName", f_name);
    	
    	String propertiesJson = gson.toJson(properties);
    	file = new File("/home/dennis/WebstormProjects/SouffleProfGui/json/properties.json");
    	try {
    		FileWriter fw = new FileWriter(file);
    		BufferedWriter bw = new BufferedWriter(fw);
    		bw.write(propertiesJson);
    		bw.close();
    	} catch (IOException e) {
    		e.printStackTrace();
    	}
    	
    	// Output rel_table_state
    	Arrays.sort(rel_table_state, DataComparator.getComparator(sortDir, DataComparator.TIME));
    	String[][] rel = out.formatTable(rel_table_state, precision);
    	String relJson = gson.toJson(rel);
    	file = new File("/home/dennis/WebstormProjects/SouffleProfGui/json/rel.json");
    	try {
    		FileWriter fw = new FileWriter(file);
    		BufferedWriter bw = new BufferedWriter(fw);
    		bw.write(relJson);
    		bw.close();
    	} catch (IOException e) {
    		e.printStackTrace();
    	}
    	
    	// Output rul_table_state
    	Arrays.sort(rul_table_state, DataComparator.getComparator(sortDir, DataComparator.TIME));
    	String[][] rul = out.formatTable(rul_table_state, precision);
    	String rulJson = gson.toJson(rul);
    	file = new File("/home/dennis/WebstormProjects/SouffleProfGui/json/rul.json");
    	try {
    		FileWriter fw = new FileWriter(file);
    		BufferedWriter bw = new BufferedWriter(fw);
    		bw.write(rulJson);
    		bw.close();
    	} catch (IOException e) {
    		e.printStackTrace();
    	}
    	
    	quit();
    }
    
    public void runProf() {
        BufferedReader br = new BufferedReader(new InputStreamReader(System.in));

        if (!loaded && !f_name.isEmpty()) {
            System.out.println("Error: File cannot be loaded");
            return;
        }

        if (loaded) {
            System.out.println("SouffleProf v2.1.8");
            top();
        }

        while (true) {

            if (!loaded) {
                loadMenu();
                if (!f_name.isEmpty()) {
                    System.out.println("Error loading file.");
                }
            }

            String input;
            try {
                System.out.print("> ");
                input = br.readLine();
                if (input == null) { 
                    return;
                }
            } catch(IOException io) { 
                System.out.println("Error reading command.");
                return;
            } 

            String[] c = input.split("\\s+");

            if (c[0].equals("q") || c[0].equals("quit")) {
                quit();
                break;
            } else if (c[0].equals("load") || c[0].equals("open")) {
                if (c.length == 2) {
                    load(c[0], c[1]);
                } else {
                    loadMenu();
                }
            } else if (c[0].equals("stop") && alive) {
                live_reader.stopRead();
                this.alive = false;
            } else if (c[0].equals("save")) {
                if (c.length == 1) {
                    System.out.println("Enter file name to save.");
                } else if (c.length == 2) {
                    save(c[1]);
                }
            } else if (c[0].equals("sort")) {
                if (c.length == 2 && Integer.parseInt(c[1]) < 7) {
                    sort_col = Integer.parseInt(c[1]);
                } else {
                    System.out.println("Invalid column, please select a number between 0 and 6.");
                }
            } else {
                runCommand(c);
            }
        }
    }

    private void loadMenu() {
        System.out.println("Please 'load' a file or 'open' from Previous Runs.");
        System.out.println("Previous Runs:");
        File folder = new File("old_runs");
        if (folder.exists()) {
            File[] listOfFiles = folder.listFiles();
            for (int i = 0; i < listOfFiles.length; i++) {
                if (listOfFiles[i].isFile()) {
                    System.out.println("- " + listOfFiles[i].getName());
                }
            }
        }
    }

    public void quit() {
        if (alive && loaded) {
            live_reader.stopRead();
        }
    }

    /*private void unser() {
        ProgramRun runser = null;
        try {
            FileInputStream fileIn = new FileInputStream("/tmp/employee.ser");
            ObjectInputStream in = new ObjectInputStream(fileIn);
            runser = (ProgramRun) in.readObject();
            in.close();
            fileIn.close();
            ProgramRun run = out.getProgramRun();
            run = runser;
        } catch (IOException i) {
            i.printStackTrace();
            return;
        } catch (ClassNotFoundException c) {
            System.out.println("Class not found");
            return;
        }
    }

    private void ser() {
        try {
            FileOutputStream fileOut
            = new FileOutputStream("/tmp/employee.ser");
            ObjectOutputStream out = new ObjectOutputStream(fileOut);
            ProgramRun programRun = this.out.getProgramRun();
            out.writeObject(programRun);
            out.close();
            fileOut.close();
            System.out.printf("Serialized data is saved in /tmp/employee.ser");
        } catch (IOException i) {
            i.printStackTrace();
        }
    }*/

    private void save(String save_name) {
        if (loaded) {
        	ProgramRun run = out.getProgramRun();
            Reader saver = new Reader(this.f_name, run, false, false);
            saver.save(save_name);
            System.out.println("Save success.");
        } else {
            System.out.println("Save failed.");
        }
    }

    private void load(String method, String load_file) {
        ProgramRun new_run = new ProgramRun();
        String f_name = load_file;
        if (method.equals("open")) {
            f_name = "old_runs/" + f_name;
        }
        Reader loader = new Reader(f_name, new_run, false, false);
        loader.readFile();
        if (loader.isLoaded()) {
            System.out.println("Load success");
            ProgramRun run = out.getProgramRun();
            run = new_run;
            this.loaded = true;
            this.f_name = f_name;
            if (alive) {
                live_reader.stopRead();
                this.alive = false;
            }
            top();
        } else {
            System.out.println("Error: File not found");
        }
    }

    private void help() {
        System.out.println("\nAvailable profiling commands:");
        System.out.print((String.format("  %-30s%-5s %-10s\n", "rel", "-",
                "display relation table.")));
        System.out.print((String.format("  %-30s%-5s %-10s\n", "rel <relation id>",
                "-", "display all rules of given relation.")));
        System.out.print((String.format("  %-30s%-5s %-10s\n", "rul", "-",
                "display rule table")));
        System.out.print((String.format("  %-30s%-5s %-10s\n", "rul <rule id>", "-",
                "display all version of given rule.")));
        System.out.print((String.format("  %-30s%-5s %-10s\n", "rul id", "-",
                "display all rules names and ids.")));
        System.out.print((String.format("  %-30s%-5s %-10s\n", "rul id <rule id>",
                "-", "display the rule name for the given rule id.")));
        System.out.print((String.format("  %-30s%-5s %-10s\n",
                "graph <relation id> <type>", "-",
                "graph the relation by type(tot_t/copy_t/tuples).")));
        System.out.print((String.format("  %-30s%-5s %-10s\n",
                "graph <rule id> <type>", "-",
                "graph the rule (C rules only)  by type(tot_t/tuples).")));
        System.out.print((String.format("  %-30s%-5s %-10s\n",
                "graph ver <rule id> <type>", "-",
                "graph the rule versions (C rules only) by type(tot_t/tuples).")));
        System.out.print((String.format("  %-30s%-5s %-10s\n", "top", "-",
                "display top-level summary of program run.")));
        System.out.print((String.format("  %-30s%-5s %-10s\n", "help", "-",
                "print this.")));

        System.out.println("\nInteractive mode only commands:");
        System.out.print((String.format("  %-30s%-5s %-10s\n", "load <filename>",
                "-", "load the given new log file.")));
        System.out.print((String.format("  %-30s%-5s %-10s\n", "open", "-",
                "list stored program run log files.")));
        System.out.print((String.format("  %-30s%-5s %-10s\n", "open <filename>",
                "-", "open the given stored log file.")));
        System.out.print((String.format("  %-30s%-5s %-10s\n", "save <filename>",
                "-", "store a log file.")));
        System.out.print((String.format("  %-30s%-5s %-10s\n", "stop", "-",
                "stop running live.")));
        System.out.print((String.format("  %-30s%-5s %-10s\n", "sort <col number>",
                "-", "sets sorting to be by given column number (0 indexed).")));
        System.out.print((String.format("  %-30s%-5s %-10s\n", "q", "-",
                "exit program.")));
    }

    private void top() {
    	ProgramRun run = out.getProgramRun();
        System.out.println("\n Total runtime: " + run.getRuntime());
        System.out.println("\n Total number of new tuples: " + run.formatNum(precision, run.getTotNumTuples()));
    }

    /**
     rul table :
     ROW[0] = TOT_T
     ROW[1] = NREC_T
     ROW[2] = REC_T
     ROW[3] = COPY_T
     ROW[4] = TUPLES
     ROW[5] = REL NAME
     ROW[6] = ID
     */
    private void rel(String c) {

        switch (sort_col) {
        case 0:
            Arrays.sort(rel_table_state,
                    DataComparator.getComparator(sortDir, DataComparator.TIME));
            break;
        case 1:
            Arrays.sort(rel_table_state,
                    DataComparator.getComparator(sortDir, DataComparator.NR_T));
            break;
        case 2:
            Arrays.sort(rel_table_state, DataComparator.getComparator(sortDir, DataComparator.R_T));
            break;
        case 3:
            Arrays.sort(rel_table_state, DataComparator.getComparator(sortDir, DataComparator.C_T));
            break;
        case 4:
            Arrays.sort(rel_table_state, DataComparator.getComparator(sortDir, DataComparator.TUP));
            break;
        case 5:
            Arrays.sort(rel_table_state, DataComparator.getComparator(sortDir, DataComparator.ID));
            break;
        case 6:
            Arrays.sort(rel_table_state, DataComparator.getComparator(sortDir, DataComparator.NAME));
            break;
        default:
            Arrays.sort(rel_table_state,
                    DataComparator.getComparator(sortDir, DataComparator.TIME));
            break;
        }

        String[][] table = out.formatTable(rel_table_state, precision);
        System.out.print(String.format(" ----- Relation Table -----\n"));
        System.out.print(String.format("%8s%8s%8s%8s%15s%6s%1s%-25s\n\n", 
                "TOT_T", "NREC_T", "REC_T", "COPY_T", "TUPLES", "ID", "", "NAME"));
        for (final String[] row : table) {
            String out;
            out = String.format("%8s%8s%8s%8s%15s%6s%1s%-5s\n",
                    (row[0]), (row[1]), (row[2]),
                    (row[3]), row[4], row[6], "", row[5]);
            System.out.print(out);
        }
    }

    /***
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
     */
    private void rul(String c) {
        switch (sort_col) {
        case 0:
            Arrays.sort(rul_table_state,
                    DataComparator.getComparator(sortDir, DataComparator.TIME));
            break;
        case 1:
            Arrays.sort(rul_table_state,
                    DataComparator.getComparator(sortDir, DataComparator.NR_T));
            break;
        case 2:
            Arrays.sort(rul_table_state, DataComparator.getComparator(sortDir, DataComparator.R_T));
            break;
        case 3:
            Arrays.sort(rul_table_state, DataComparator.getComparator(sortDir, DataComparator.C_T));
            break;
        case 4:
            Arrays.sort(rul_table_state, DataComparator.getComparator(sortDir, DataComparator.TUP));
            break;
        case 5:
            Arrays.sort(rul_table_state, DataComparator.getComparator(sortDir, DataComparator.ID));
            break;
        case 6:
            Arrays.sort(rul_table_state, DataComparator.getComparator(sortDir, DataComparator.NAME));
            break;
        default:
            Arrays.sort(rul_table_state,
                    DataComparator.getComparator(sortDir, DataComparator.TIME));
            break;
        }
        
        String[][] table = out.formatTable(rul_table_state, precision);
        System.out.print("  ----- Rule Table -----\n");
        System.out.print(String.format("%8s%8s%8s%8s%15s    %-5s\n\n", "TOT_T",
                "NREC_T", "REC_T", "COPY_T", "TUPLES", "ID RELATION"));
        for (final String[] row : table) {

            String out = String.format("%8s%8s%8s%8s%15s%8s %-25s\n",
                    (row[0]), (row[1]), (row[2]),
                    (row[3]), row[4], row[6], row[7]);
            System.out.print(out);
        }
    }

    private void id(String col) {
        String[][] table = out.formatTable(rul_table_state, precision);
        if (col.equals("0")) {
            System.out.print(String.format("%7s%2s%-25s\n\n", "ID", "", "NAME"));
            Arrays.sort(table,
                    DataComparator.getComparator(sortDir, DataComparator.NAME));
            for (final String[] row : table) {
                System.out.print(String.format("%7s%2s%-25s\n", row[6], "", row[5]));
            }
        } else {
            for (final String[] row : table) {
                if (((String) row[6]).equals(col)) {
                    System.out.print(String.format("%7s%2s%-25s\n", row[6], "", row[5]));
                }
            }
        }
    }

    private void relRul(String str) {
        switch (sort_col) {
        case 0:
            Arrays.sort(rul_table_state,
                    DataComparator.getComparator(sortDir, DataComparator.TIME));
            break;
        case 1:
            Arrays.sort(rul_table_state,
                    DataComparator.getComparator(sortDir, DataComparator.NR_T));
            break;
        case 2:
            Arrays.sort(rul_table_state,
                    DataComparator.getComparator(sortDir, DataComparator.R_T));
            break;
        case 3:
            Arrays.sort(rul_table_state,
                    DataComparator.getComparator(sortDir, DataComparator.C_T));
            break;
        case 4:
            Arrays.sort(rul_table_state,
                    DataComparator.getComparator(sortDir, DataComparator.TUP));
            break;
        case 5:
            Arrays.sort(rul_table_state,
                    DataComparator.getComparator(sortDir, DataComparator.ID));
            break;
        case 6:
            Arrays.sort(rul_table_state,
                    DataComparator.getComparator(sortDir, DataComparator.NAME));
            break;
        default:
            Arrays.sort(rul_table_state,
                    DataComparator.getComparator(sortDir, DataComparator.TIME));
            break;
        }
        String[][] rul_table = out.formatTable(rul_table_state, precision);
        String[][] rel_table = out.formatTable(rel_table_state, precision);
        System.out.print("  ----- Rules of a Relation -----\n");
        System.out.print(String.format("%8s%8s%8s%8s%10s%8s %-25s\n\n", "TOT_T",
                "NREC_T", "REC_T", "COPY_T", "TUPLES", "ID", "NAME"));
        String name = "";
        for (final String[] row : rel_table) {
            if (((String) row[5]).equals(str) || ((String) row[6]).equals(str)) {
                System.out.print(String.format("%8s%8s%8s%8s%10s%8s %-25s\n",
                        (row[0]), (row[1]),
                        (row[2]), (row[3]),
                        row[4], row[6], row[5]));
                name = (String) row[5];
                break;
            }
        }
        System.out.print( " ---------------------------------------------------------\n");
        for (final String[] row : rul_table) {
            if (((String) row[7]).equals(name)) {
                System.out.print(String.format("%8s%8s%8s%8s%10s%8s %-25s\n",
                        (row[0]), (row[1]),
                        (row[2]), (row[3]),
                        row[4], row[6], row[7]));
            }
        }
        String src = "";
        ProgramRun run = out.getProgramRun();
        if (run.getRelation(name) != null) {
            src = run.getRelation(name).getLocator();
        }
        System.out.print("\nSrc locator: " + src + "\n\n");
        for (final String[] row : rul_table) {
            if (((String) row[7]).equals(name)) {
                System.out.print(
                        (String.format("%7s%2s%-25s\n", row[6], "", row[5])));
            }
        }
    }
    private void verRul(String str) {

        if (!str.contains(".")) {
            System.out.println("Rule does not exist");
            return;
        }
        String[] part = str.split("\\.", 2);
        String strRel = "R" + part[0].substring(1);
        Object[][] ver_table = out.getVersions(strRel, str);
        switch (sort_col) {
        case 0:
            Arrays.sort(ver_table,
                    DataComparator.getComparator(sortDir, DataComparator.TIME));
            break;
        case 1:
            Arrays.sort(ver_table,
                    DataComparator.getComparator(sortDir, DataComparator.NR_T));
            break;
        case 2:
            Arrays.sort(ver_table,
                    DataComparator.getComparator(sortDir, DataComparator.R_T));
            break;
        case 3:
            Arrays.sort(ver_table,
                    DataComparator.getComparator(sortDir, DataComparator.C_T));
            break;
        case 4:
            Arrays.sort(ver_table,
                    DataComparator.getComparator(sortDir, DataComparator.TUP));
            break;
        case 5:
            Arrays.sort(ver_table,
                    DataComparator.getComparator(sortDir, DataComparator.ID));
            break;
        case 6:
            Arrays.sort(ver_table,
                    DataComparator.getComparator(sortDir, DataComparator.NAME));
            break;
        default:
            Arrays.sort(ver_table,
                    DataComparator.getComparator(sortDir, DataComparator.TIME));
            break;
        }
        String[][] rul_table = out.formatTable(rul_table_state, precision);
        System.out.print("  ----- Rule Versions Table -----\n");
        System.out.print(String.format("%8s%8s%8s%8s%10s%6s   %-5s\n\n", "TOT_T",
                "NREC_T", "REC_T", "COPY_T", "TUPLES", "VER", "ID RELATION"));
        boolean found = false;
        for (final String[] row : rul_table) {
            if (((String) row[6]).equals(str)) {
                System.out.print(String.format("%8s%8s%8s%8s%10s%6s%7s %-25s\n",
                        (row[0]), (row[1]),
                        (row[2]), (row[3]),
                        row[4], "", row[6], row[7]));
                found = true;
            }
        }
        System.out.print(" ---------------------------------------------------------\n");
        for (final Object[] row : ver_table) {
            System.out.print(String.format("%8s%8s%8s%8s%10s%6s%7s %-25s\n",
                    out.formatTime(row[0]), out.formatTime(row[1]), out.formatTime(row[2]),
                    out.formatTime(row[3]), row[4], row[8], row[6],
                    row[7]));

        }
        if (found) {
            if (ver_table.length > 0) {
                System.out.print("\nSrc locator: " + ver_table[0][9] + "\n\n");
            } else if (rul_table.length > 0){
                System.out.print("\nSrc locator-: " + rul_table[0][10] + "\n\n");
            }
        }
        for (final String[] row : rul_table) {
            if (((String) row[6]).equals(str)) {
                System.out.print(
                        (String.format("%7s%2s%-25s\n", row[6], "", row[5])));
            }
        }
    }

    private void iterRel(String c, String col) {
        String[][] table = out.formatTable(rel_table_state, -1);
        List<Iteration> iter;
        for (final String[] row : table) {
            if (((String) row[6]).equals(c) || ((String) row[5]).equals(c)) {
                System.out.print(
                        (String.format("%4s%2s%-25s\n\n", row[6], "", row[5])));
                ProgramRun run = out.getProgramRun();
                iter = run.getRelation_map().get((String) row[5])
                        .getIterations();
                List<Object> list = new ArrayList<Object>();
                if (col.equals("tot_t")) {
                    for (Iteration i : iter) {
                        list.add(i.getRuntime());
                    }
                    System.out.print(
                            (String.format("%4s   %-6s\n\n", "NO", "RUNTIME")));
                    graphD(list);
                } else if (col.equals("copy_t")) {
                    for (Iteration i : iter) {
                        list.add(i.getCopy_time());
                    }
                    System.out.print(
                            (String.format("%4s   %-6s\n\n", "NO", "COPYTIME")));
                    graphD(list);
                } else if (col.equals("tuples")) {
                    for (Iteration i : iter) {
                        list.add(i.getNum_tuples());
                    }
                    System.out.print(
                            (String.format("%4s   %-6s\n\n", "NO", "TUPLES")));
                    graphL(list);
                }
                break;
            }
        }

    }

    private void iterRul(String c, String col) {
        String[][] table = out.formatTable(rul_table_state, precision);
        List<Iteration> iter;
        for (String[] row : table) {
            if (((String) row[6]).equals(c)) {
                System.out.print(
                        (String.format("%6s%2s%-25s\n\n", row[6], "", row[5])));
                ProgramRun run = out.getProgramRun();
                iter = run.getRelation_map().get((String) row[7])
                        .getIterations();
                List<Object> list = new ArrayList<Object>();
                if (col.equals("tot_t")) {
                    for (Iteration i : iter) {
                        Boolean add = false;
                        Double tot_time = 0.0;
                        for (Rule rul : i.getRul_rec().values()) {
                            if (rul.getId().equals(c)) {
                                tot_time += rul.getRuntime();
                                add = true;
                            }
                        }
                        if (add) {
                            list.add(tot_time);
                        }
                    }
                    System.out.print(
                            (String.format("%4s   %-6s\n\n", "NO", "RUNTIME")));
                    graphD(list);
                } else if (col.equals("tuples")) {
                    for (Iteration i : iter) {
                        Boolean add = false;
                        Long tot_num = 0L;
                        for (Rule rul : i.getRul_rec().values()) {
                            if (rul.getId().equals(c)) {
                                tot_num += rul.getNum_tuples();
                                add = true;
                            }
                        }
                        if (add) {
                            list.add(tot_num);
                        }
                    }
                    System.out.print(
                            (String.format("%4s   %-6s\n\n", "NO", "TUPLES")));
                    graphL(list);
                }
                break;
            }
        }
    }

    private void verGraph(String c, String col) {
        if (!c.contains(".")) {
            System.out.println("Rule does not exist");
            return;
        }
        String[] part = c.split("\\.", 2);
        String strRel = "R" + part[0].substring(1);
        Object[][] ver_table = out.getVersions(strRel, c);
        System.out.print((String.format("%6s%2s%-25s\n\n", ver_table[0][6], "",
                ver_table[0][5])));
        List<Object> list = new ArrayList<Object>();
        if (col.equals("tot_t")) {
            for (Object[] row : ver_table) {
                list.add(row[0]);
            }
            System.out.print((String.format("%4s   %-6s\n\n", "NO", "RUNTIME")));
            graphD(list);

        } else if (col.equals("copy_t")) {
            for (Object[] row : ver_table) {
                list.add(row[3]);
            }
            System.out.print((String.format("%4s   %-6s\n\n", "NO", "COPYTIME")));
            graphD(list);

        } else if (col.equals("tuples")) {
            for (Object[] row : ver_table) {
                list.add(row[4]);
            }
            System.out.print((String.format("%4s   %-6s\n\n", "NO", "TUPLES")));
            graphL(list);
        }
    }

    private void graphD(List<Object> list) {
        Double max = 0.0;
        Double time;
        for (Object o : list) {
            time = (Double) o;
            if (Double.compare(time, max) >= 0) {
                max = time;
            }
        }
        int i = 0;
        for (Object o : list) {
            time = (Double) o;
            int len = (int) ((time / max) * (67));
            char[] chars = new char[len];
            Arrays.fill(chars, '*');
            String bar = new String(chars);
            System.out.print((String.format("%4d %10.8f | %s\n", i++, (time), bar)));
        }
    }

    private void graphL(List<Object> list) {
        Long max = 0L;
        Long num;
        Long tot = 0L;
        for (Object o : list) {
            num = (Long) o;
            if (num > max) {
                max = num;
            }
        }
        int i = 0;
        for (Object o : list) {
            num = (Long) o;
            tot += num;
            int len = (int) ((float) ((float) num / (float) max) * (64));
            char[] chars = new char[len];
            Arrays.fill(chars, '*');
            String bar = new String(chars);
            System.out.print(
                    (String.format("%4d %8s | %s\n", i++, out.formatNum(precision, num), bar)));
        }      
    }
}
