/**
 * @file interrupts.cpp
 * @author Sasisekhar Govind
 * @author Wenxuan Han 101256669
 * @author Tony Yao 101298080
 * 
 */

#include "interrupts_101256669_101298080.hpp"


const unsigned int TIME_QUANTUM = 100; // Time quantum for Round Robin

std::tuple<std::string /* add std::string for bonus mark */ > run_simulation(std::vector<PCB> list_processes) {

    std::vector<PCB> ready_queue;   //The ready queue of processes
    std::vector<PCB> wait_queue;    //The wait queue of processes
    std::vector<PCB> job_list;      //A list to keep track of all the processes. This is similar
                                    //to the "Process, Arrival time, Burst time" table that you
                                    //see in questions. You don't need to use it, I put it here
                                    //to make the code easier :).

    unsigned int current_time = 0;
    unsigned int time_spent_in_quantum = 0; // Track time spent in current quantum
    PCB running;

    //Initialize an empty running process
    idle_CPU(running);

    std::string execution_status;

    //make the output table (the header row)
    execution_status = print_exec_header();

    //Loop while till there are no ready or waiting processes.
    //This is the main reason I have job_list, you don't have to use it.
    while(!all_process_terminated(job_list) || job_list.empty()) {

        //Inside this loop, there are three things you must do:
        // 1) Populate the ready queue with processes as they arrive
        // 2) Manage the wait queue
        // 3) Schedule processes from the ready queue

        //Population of ready queue is given to you as an example.
        //Go through the list of proceeses
        for(auto &process : list_processes) {
            if(process.arrival_time == current_time) {//check if the AT = current time
                //if so, assign memory and put the process into the ready queue
                assign_memory(process);

                process.state = READY;  //Set the process state to READY
                ready_queue.push_back(process); //Add the process to the ready queue
                job_list.push_back(process); //Add it to the list of processes

                execution_status += print_exec_status(current_time, process.PID, NEW, READY);
            }
        }

        ///////////////////////MANAGE WAIT QUEUE/////////////////////////
        //This mainly involves keeping track of how long a process must remain in the ready queue
        for(int i = wait_queue.size() - 1; i >= 0; i--) { // Backwards iteration to allow erasing without index errors
            wait_queue[i].io_duration_remaining--; // Decrement remaining I/O duration

            if(wait_queue[i].io_duration_remaining == 0) { // Check if I/O is done
                // if so, update state and move to ready queue
                states old_state = WAITING;
                wait_queue[i].state = READY;
                ready_queue.push_back(wait_queue[i]);
                
                // Update job list and execution status
                sync_queue(job_list, wait_queue[i]);
                execution_status += print_exec_status(current_time, wait_queue[i].PID, old_state, READY);
                
                // Remove from wait queue
                wait_queue.erase(wait_queue.begin() + i);
            }
        }
        /////////////////////////////////////////////////////////////////

        //////////////////////////SCHEDULER//////////////////////////////
        // Preemption Check 1: External Priority
        if (running.state == RUNNING && !ready_queue.empty()) { // Can only preempt if there's a running process and ready queue is not empty
            // Find the index of the process with the smallest PID in ready queue (highest priority)
            int smallest_PID_index = 0;
            for (int i = 1; i < ready_queue.size(); i++) {
                if (ready_queue[i].PID < ready_queue[smallest_PID_index].PID) {
                    smallest_PID_index = i;
                }
            }

            if (running.PID > ready_queue[smallest_PID_index].PID) { // Check if the running job has a lower priority (higher PID)
                // Preempt the running process
                states old_state = running.state;
                running.state = READY;
                ready_queue.push_back(running); 

                // Update job list and execution status
                sync_queue(job_list, running);
                execution_status += print_exec_status(current_time, running.PID, old_state, READY); 
                
                idle_CPU(running); // Set CPU to idle
                time_spent_in_quantum = 0; // Reset quantum timer
            }
        }
    
        // Preemption Check 2: Time Quantum
        if(running.state == RUNNING && time_spent_in_quantum == TIME_QUANTUM) { // Check for Time Quantum expiration
            // Preempt the running process
            states old_state = running.state;
            running.state = READY;
            ready_queue.push_back(running); 
            
            // Update job list and execution status
            sync_queue(job_list, running);
            execution_status += print_exec_status(current_time, running.PID, old_state, READY); 
            
            idle_CPU(running); // Set CPU to idle
            time_spent_in_quantum = 0; // Reset quantum timer
        }

        // After preemption checks, scheduler sends dispatcher to perform context switch if CPU is idle (EP + RR)
        if(running.state != RUNNING && !ready_queue.empty()) { 
            // Find the index of the process with the smallest PID in ready queue
            int smallest_PID_index = 0;
            for (int i = 1; i < ready_queue.size(); i++) {
                if (ready_queue[i].PID < ready_queue[smallest_PID_index].PID) {
                    smallest_PID_index = i;
                }
            }

            // Get a copy of the process and remove it from the ready queue
            PCB next = ready_queue[smallest_PID_index];
            ready_queue.erase(ready_queue.begin() + smallest_PID_index);

            // Update the running process
            states old_state = next.state;
            next.state = RUNNING;
            running = next;
            
            // Update job list and execution status
            sync_queue(job_list, running);
            execution_status += print_exec_status(current_time, running.PID, old_state, RUNNING);

            time_spent_in_quantum = 0; // Reset time quantum timer
        }
     
        /////////////////////////////////////////////////////////////////

        //////////////////////////RUNNING////////////////////////////////
        if(running.state == RUNNING) {
            running.remaining_time--; // Decrement remaining time
            time_spent_in_quantum++; // Update quantum timer

            // Check for I/O 
            if(running.io_freq > 0 && // 1. I/O frequency set
                running.remaining_time != running.processing_time && // 2. Not first tick
                (running.processing_time - running.remaining_time) % running.io_freq == 0 && // 3. I/O frequency reached
                running.remaining_time > 0) { // 4. Process still running

                // Update the running process    
                states old_state = RUNNING;
                running.state = WAITING;
                
                // Add process to wait queue
                running.io_duration_remaining = running.io_duration + 1; // +1 because we decrement at the start of the wait queue management
                wait_queue.push_back(running);
                
                // Update job list and execution status
                sync_queue(job_list, running);
                execution_status += print_exec_status(current_time + 1, running.PID, old_state, WAITING);
                
                idle_CPU(running); // Set CPU to idle
                time_spent_in_quantum = 0; // Reset quantum timer
            }
            // Process finishes execution
            else if(running.remaining_time == 0) {
                // Update the running process
                states old_state = RUNNING;
                
                // Update execution status and terminate process (free memory, sync job list, update state)
                execution_status += print_exec_status(current_time + 1, running.PID, old_state, TERMINATED); 
                terminate_process(running, job_list);

                idle_CPU(running); // Set CPU to idle
                time_spent_in_quantum = 0; // Reset quantum timer
            }
        }
        /////////////////////////////////////////////////////////////////

        current_time++; // Update time
    }
    
    //Close the output table
    execution_status += print_exec_footer();

    return std::make_tuple(execution_status);
}


int main(int argc, char** argv) {

    //Get the input file from the user
    if(argc != 2) {
        std::cout << "ERROR!\nExpected 1 argument, received " << argc - 1 << std::endl;
        std::cout << "To run the program, do: ./interrupts <your_input_file.txt>" << std::endl;
        return -1;
    }

    //Open the input file
    auto file_name = argv[1];
    std::ifstream input_file;
    input_file.open(file_name);

    //Ensure that the file actually opens
    if (!input_file.is_open()) {
        std::cerr << "Error: Unable to open file: " << file_name << std::endl;
        return -1;
    }

    //Parse the entire input file and populate a vector of PCBs.
    //To do so, the add_process() helper function is used (see include file).
    std::string line;
    std::vector<PCB> list_process;
    while(std::getline(input_file, line)) {
        auto input_tokens = split_delim(line, ", ");
        auto new_process = add_process(input_tokens);
        list_process.push_back(new_process);
    }
    input_file.close();

    //With the list of processes, run the simulation
    auto [exec] = run_simulation(list_process);

    write_output(exec, "execution.txt");

    return 0;
}