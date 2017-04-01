/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2016, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file SignalHandler.h
 *
 * A signal handler for Souffle's interpreter and compiler.
 *
 ***********************************************************************/

#pragma once 
#include <string>
#include <iostream>
#include <assert.h>
#include <signal.h>
#include <stdio.h>

namespace souffle {

/**
 * Class SignalHandler captures signals
 * and reports the context where the signal occurs. 
 * The signal handler is implemented as a singleton. 
 */
class SignalHandler {
private: 
    /**
     * Signal handler for various types of signals.
     */ 
    static void handler(int signal) {
      assert(m_singleton != nullptr && "m_singleton initialization failed"); 
      std::string &msg = m_singleton->m_msg; 
      std::string error; 
      switch(signal) { 
      case SIGINT: 
         error = "Interrupt";   
         break; 
      case SIGFPE: 
         error = "Floating-point arithmetic exception"; 
         break; 
      case SIGSEGV: 
         error = "Segmentation violation"; 
         break; 
      default: 
         error = "Unknown";
         break;
      } 
      if (msg != "") {
         std::cerr << error << " signal in rule:\n" << msg << std::endl;
      } else {
         std::cerr << error << " signal." << std::endl;
      } 
      exit(1);
    }

    SignalHandler() { 
    } 
public:
    static SignalHandler *instance() { 
        if (m_singleton == nullptr) {
           // create singleton instance
           m_singleton = new SignalHandler(); 
         
           // register signals
           signal(SIGFPE, handler);  // floating point exception
           signal(SIGINT, handler);  // user interrupts 
           signal(SIGSEGV, handler); // memory issues
        } 
        return m_singleton;
    }

    // set signal message 
    void setMsg(const std::string &msg) { 
       m_msg = msg; 
    } 
private:
    std::string m_msg; 
    static SignalHandler *m_singleton;
};

} // end of namespace 
