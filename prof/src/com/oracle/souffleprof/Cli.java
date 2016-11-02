/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

package com.oracle.souffleprof;

/**
 * Command Line Interface 
 * 
 * Provides an interface to parse the execution arguments of the souffle profiler 
 */

public class Cli {

    /**
     * List of arguments
     */
    private String[] args;

    /**
     * Constructor for the command line interface
     *  
     * @param args arguments of the static main functions
     */
    public Cli(String[] args) {
        this.args = args;
    }

    /**
     * Error handler
     * 
     * Prints usage of souffle profiler
     */
    public void error() {
        System.out.println("java -jar souffleprof.jar [-f|-j <file> [-c <command>] [-l]] [-h]"); 
        System.exit(1); 
    }


    /**
     * Parse command line parameters 
     * 
     * Arguments are checked and the text-user interface of the profiler is invoked.
     */
    public void parse() {
        /** 
         * Profile command provided by argument interface 
         */ 
        String [] commands = {};

        /**
         * Filename 
         */ 
        String filename = "";

        /** 
         * Alive flag denoting whether a completed profile is processed or a running one 
         */ 
        boolean alive = false;

        int i=0;

        while (i < args.length && args[i].startsWith("-")) {
            String arg = args[i++]; 

            if (arg.equals("-h")) { 
                System.out.println("SouffleProf Alpha3 (2 Feb 2015)");
                System.out.println("");
                error(); 
            } else if (arg.equals("-c")) { 
                if (i  <= args.length) { 
                    commands = args[i++].split("\\s+"); 
                } else { 
                    System.out.println("Parameter for option -c missing!"); 
                    error(); 
                }
            } else if (arg.equals("-f")) { 
                if (i  <= args.length) { 
                    filename = args[i++]; 
                } else { 
                    System.out.println("Parameter for option -f missing!"); 
                    error(); 
                }
            } else if (arg.equals("-j")) { 
                if (i  <= args.length) { 
                    filename = args[i++]; 
                } else { 
                    System.out.println("Parameter for option -j missing!"); 
                    error(); 
                }
            } else if (arg.equals("-l")) {
                alive = true; 
            } else {
                System.out.println("Unknown argument " + args[i]); 
                error(); 
            }
        }

        /**
         * Invoke text user interface
         */
        if (commands.length > 0) { 
            new Tui(filename, alive).runCommand(commands); 
        } else {
        	if (args[0].equals("-j")) {
        		// Output data models to SouffleProfGui in JSON format
        		new Tui(filename, alive).runGui();
        	} else {
        		new Tui(filename, alive).runProf();	
        	}
        }
    }
}
