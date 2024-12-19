#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include "sim_proc.h"
#include <iostream>
#include <typeinfo>
#include <tuple>
#include <string>
#include <iomanip> // For std::setw()
using namespace std;

//in the debug mode, we print RMT, ROB, IQ, and everything to check the pipeline work correctly
bool is_debug_mode = 0;
bool is_check_boolean_mode = 0;
bool is_debug_mode_counter = 0;
int debug_counter = 110;

//we doesn't implement ARF since we focus on instr. timing info not the calculation of register
/* ARF_and_RMT_entries = 67 is defined by the MIP like ISA: 32 integer registers, 32 floating-point registers, 
the HI and LO registers (for results of integer multiplication/divide), 
and the FCC register (floating-point condition code register). */
int ARF_and_RMT_entries = 67; //test -> correct:67
uint64_t dynamic_instr_count = 0; //based on the SPEC, the first one should be 0
uint64_t num_of_retire_instr = 0;
unsigned long int global_cycle = 0;//start from 0(based on SPEC)
int total_instr_count = 10000;

//output
// double dynamic_instr_count = 0;
//double cycles = 0;
double IPC = 0;  

/* ======================== The above is main function, the following are my class and sub-function ======================== */
class Instructions{
public:
    int seq_no = -1;   //initialize //instruction sequence number(i.e.i0, i1, and i2 -> 0, 1, and 2)
    string op_type = "-";   //operation type
    string src1 = "-";      //source register 1(might be immediate value, -1)
    string src2 = "-"; 
    string dst = "-";       //destination register(if no destination -1, might be branch)

    //each stage's cycle, indcating the time when the instruction go in to that stage
    //i.e. If i66 go to EX stage at cycle 352, then it's EX_cycle is 352 
    unsigned long int FE_cycle = 0;
    unsigned long int DE_cycle = 0;
    unsigned long int RN_cycle = 0;
    unsigned long int RR_cycle = 0;
    unsigned long int DI_cycle = 0;
    unsigned long int IS_cycle = 0;
    unsigned long int EX_cycle = 0;
    unsigned long int WB_cycle = 0;
    unsigned long int RT_cycle = 0;
    unsigned long int End_cycle = 0;

    // Setter methods
    void set_seq_no(int seq) { seq_no = seq; }
    void set_op_type(const std::string &op) { op_type = op; }
    void set_src1(const std::string &src) { src1 = src; }
    void set_src2(const std::string &src) { src2 = src; }
    void set_dst(const std::string &dest) { dst = dest; }

    void set_FE_cycle(unsigned long int FE) { FE_cycle = FE; }
    void set_DE_cycle(unsigned long int DE) { DE_cycle = DE; }
    void set_RN_cycle(unsigned long int RN) { RN_cycle = RN; }
    void set_RR_cycle(unsigned long int RR) { RR_cycle = RR; }
    void set_DI_cycle(unsigned long int DI) { DI_cycle = DI; }
    void set_IS_cycle(unsigned long int IS) { IS_cycle = IS; }
    void set_EX_cycle(unsigned long int EX) { EX_cycle = EX; }
    void set_WB_cycle(unsigned long int WB) { WB_cycle = WB; }
    void set_RT_cycle(unsigned long int RT) { RT_cycle = RT; }
    void set_End_cycle(unsigned long int End) { End_cycle = End; }

    string get_op_type() {return op_type;}

    void print_instruction_info(){
        unsigned long int FE_duration = DE_cycle - FE_cycle; // Fetch duration
        unsigned long int DE_duration = RN_cycle - DE_cycle; // Decode duration
        unsigned long int RN_duration = RR_cycle - RN_cycle; // Rename duration
        unsigned long int RR_duration = DI_cycle - RR_cycle; // Register Read duration
        unsigned long int DI_duration = IS_cycle - DI_cycle; // Dispatch duration
        unsigned long int IS_duration = EX_cycle - IS_cycle; // Issue duration
        unsigned long int EX_duration = WB_cycle - EX_cycle; // Execute duration
        unsigned long int WB_duration = RT_cycle - WB_cycle; // Write Back duration
        unsigned long int RT_duration = End_cycle - RT_cycle; // Retire duration


        std::cout << seq_no << " fu{" << op_type << "} "
              << "src{" << src1 << "," << src2 << "} "
              << "dst{" << dst << "} "
              << "FE{" << FE_cycle << "," << FE_duration << "} "
              << "DE{" << DE_cycle << "," << DE_duration << "} "
              << "RN{" << RN_cycle << "," << RN_duration << "} "
              << "RR{" << RR_cycle << "," << RR_duration << "} "
              << "DI{" << DI_cycle << "," << DI_duration << "} "
              << "IS{" << IS_cycle << "," << IS_duration << "} "
              << "EX{" << EX_cycle << "," << EX_duration << "} "
              << "WB{" << WB_cycle << "," << WB_duration << "} "
              << "RT{" << RT_cycle << "," << RT_duration << "}" 
              << std::endl;

    }
};

class Issue_Queue{
public:
    int valid_bit = 0;
    string dest_tag = "-";
    int rs1_ready_bit = 0;
    string rs1_tag_or_value = "-";
    int rs2_ready_bit = 0;
    string rs2_tag_or_value = "-";

    //instr_age is not part of Issue Queue
    //this is for processor can find the oldest instr in the queue so they can be issued in a correct order
    //the smaller the instr age is, the older it is(i.e. 0 is the oldest one)
    //In my design, I use PC(i.e. i1, i2, i3) as an aid to set up the instr_age
    int instr_age = -1; //-1 means the entry in the queue is free to used

    string op_type = "-";//initialize, in this project, there are only three type of operation which is type 0, 1, and 3

    //get info function
    int get_valid_bit()             { return valid_bit; }
    string get_dest_tag()           { return dest_tag; }
    int get_rs1_ready_bit()         { return rs1_ready_bit; }
    string get_rs1_tag_or_value()   { return rs1_tag_or_value; }
    int get_rs2_ready_bit()         { return rs2_ready_bit; }
    string get_rs2_tag_or_value()   { return rs2_tag_or_value; }

    int get_instr_age()           { return instr_age;}
    string get_op_type()             { return op_type;}

    //set up function
    void set_valid_bit(int valid)              { valid_bit = valid; }
    void set_dest_tag(string tag)              { dest_tag = tag; }
    //rs1 and rs2 == register source 1 and 2, not r1 and r2
    void set_rs1_ready_bit(int ready)          { rs1_ready_bit = ready; }
    void set_rs1_tag_or_value(string value)    { rs1_tag_or_value = value; }
    void set_rs2_ready_bit(int ready)          { rs2_ready_bit = ready; }
    void set_rs2_tag_or_value(string value)    { rs2_tag_or_value = value; }
    
    void set_instr_age(int age)                { instr_age = age;}
    void set_op_type(string type)                 { op_type = type;}
};

class RMT{ // Rename Map Table
public:
    string reg_number = "0"; //randomly initialize
    int valid_bit = 0; //initialize to 0, indicate instruction might want to find the reg value at ARF(just a concept, we dont implement ARF in this simulater as we focus on instr. timing info)
    string rob_tag = "-"; // "-" tell the processor go to find the register in ARF 

    //get info function
    string get_reg_number()     { return reg_number;}
    int get_valid_bit()         { return valid_bit;}
    string get_rob_tag()        { return rob_tag;}

    //set up function
    void set_reg_number(string reg_num)     {reg_number = reg_num;}
    void set_valid_bit(int valid)           {valid_bit = valid;}
    void set_rob_tag(string tag)            {rob_tag = tag;}
};

class ROB{ //recorder buffer
public:
    string rob_number = "0";
    int value = 0;
    string dest_reg = "-"; //destionation register, i.e r1, r2, and so on, "-" represent there is no dest reg(for example: branch)
    int ready = 0;
    int exception = 0;
    int misprediction = 0;
    string PC = "-" ; //Instruction #, i.e. i1, i2, i3, and so on. "-" means not being used 


    //get info function
    string get_rob_number()     { return rob_number;}
    int get_value()             { return value; }
    string get_dest_reg()       { return dest_reg; }
    int get_ready_bit()             { return ready; }
    int get_exception()         { return exception; }
    int get_misprediction()     { return misprediction; }
    string get_PC()             { return PC; }

    //set up function
    void set_rob_number(string num)     { rob_number = num; }
    void set_value(int val)             { value = val; }
    void set_dest_reg(string reg)       { dest_reg = reg; }
    void set_ready(int r)               { ready = r; }
    void set_exception(int e)           { exception = e; }
    void set_misprediction(int m)       { misprediction = m; }
    void set_PC(string pc)              { PC = pc; }
};


class OOO_processor{
private:

public:
    // int debug_counter = 10;
    unsigned long int issue_instr_count = 0;
    unsigned long int number_of_instr_from_EX_stage = 0;
    int op_type, dest, src1, src2;  // Variables are read from trace file
    uint64_t pc; // Variable holds the pc read from input file

    //Variables are read from trace file then will pass to the stage afterward
    string *op_type_fetch, *op_type_decode, *op_type_rename, *op_type_regread, *op_type_dispatch, *op_type_issue, *op_type_execute, *op_type_writeback, *op_type_retire;
    //destionation reg
    string *dest_fetch, *dest_decode, *dest_rename, *dest_regread, *dest_dispatch, *dest_issue, *dest_execute, *dest_writeback, *dest_retire;
    //source reg
    string *src1_fetch, *src1_decode, *src1_rename, *src1_regread, *src1_dispatch, *src1_issue, *src1_execute, *src1_writeback, *src1_retire;
    string *src2_fetch, *src2_decode, *src2_rename, *src2_regread, *src2_dispatch, *src2_issue, *src2_execute, *src2_writeback, *src2_retire;
    //program counter 
    string *pc_fetch, *pc_decode, *pc_rename, *pc_regread, *pc_dispatch, *pc_issue, *pc_execute, *pc_writeback, *pc_retire;
    //instruction number 
    string *instr_num_fetch, *instr_num_decode, *instr_num_rename, *instr_num_regread, *instr_num_dispatch, *instr_num_issue, *instr_num_execute, *instr_num_writeback, *instr_num_retire;

    //executing list
    int *executing_latency_counter;
    string *op_type_executing, *dest_executing, *src1_executing, *src2_executing, *pc_executing, *instr_num_executing;
    bool *is_wakeup_dispatch, *is_wakeup_regread;

    //using the following two to detect if the pipeline is empty
    //actually detecting head = tail is enough
    /*we give re-order buffer(ROB) enough entries so we dont need to worry about the ROB is full(where head also = tail but the program is not completed)*/
    bool is_head_equals_to_tail = false; 
    bool is_issue_queue_empty = false;

    //these boolean determine if the bundle want to move forward to next pipeline stage or not
    bool is_DE_empty = true;
    bool is_RN_empty = true;
    bool is_RR_empty = true;
    bool has_ROB_free_entries = true;
    bool is_DI_empty = true;
    bool is_IQ_enough = true;

    //These two together to detect if the program is finished or not
    bool is_pipeline_empty = false;
    bool is_trace_depleted = false;

    // unsigned long int global_cycle = 1;//start from 0(based on SPEC)

    //re-order buffer, head, and tail
    ROB *rob;
    ROB *ptr_head, *ptr_tail;

    //RMT
    RMT *rmt;

    //Issue Queue
    Issue_Queue *queue;
    
    //instructions timing info
    Instructions *instruction;

    unsigned long int rob_size;
    unsigned long int issue_queue_size;
    unsigned long int width;

    //constructor function, initialize rob object, head, tail, RMT(rename map table), and issue queue
    //since most variable already setup as 0 in the class originally, therefore, we only have to setup few
    OOO_processor(unsigned long int rob_size, unsigned long int issue_queue_size, unsigned long int width){

        //assigned main function's variables to class built-in variables
        //Use "this->" syntax to ensure variable from main function pass to the varable in the class
        this->rob_size = rob_size;
        this->issue_queue_size = issue_queue_size;
        this->width = width;
        
        /* ----- set up ROB ---- */
        //rob size == the number of entry in the rob
        rob = new ROB[rob_size];

        // Initialize each ROB object in the array, setting rob_number
        for (unsigned long int i = 0; i < rob_size; i++) 
            rob[i].set_rob_number("rob" + std::to_string(i));  // Set the rob_number to the index (i)


        //inistialze head and tail, pointing to the first element
        //where it begin, it doesn't matter, I set it to rob[3] for debug purpose(this example is provided in ECE563 slide ILP1)
        ptr_head = rob + 3;
        ptr_tail = rob + 3;
        
        /* ----- set up RMT ---- */
        rmt = new RMT[ARF_and_RMT_entries]; //number of entries is defined by ISA

        for(int i = 0; i < ARF_and_RMT_entries; i++)
            rmt[i].set_reg_number("r" + std::to_string(i));

        for(int i = 0; i < ARF_and_RMT_entries; i++)
            rmt[i].set_rob_tag("-");


        /* ----- set up Issue Queue ---- */
        queue = new Issue_Queue[issue_queue_size];
        //ptr_queue_entry = queue;

        /* -----set up instruction(to get timing info)------*/
        instruction = new Instructions[total_instr_count];
        for(int i = 0; i < total_instr_count; i++){
            instruction[i].set_seq_no(i);
        }

        /* ----- set up dynamic 2D array for each stage ---- */

        /* set up register for each stage*/
        // Allocate for op_type
        op_type_fetch = new string[width];     op_type_decode = new string[width];      op_type_rename = new string[width];    
        op_type_regread = new string[width];   op_type_dispatch = new string[width];    op_type_issue = new string[width];
        op_type_execute = new string[width];   op_type_writeback = new string[width*5]; op_type_retire = new string[width];

        // Allocate for destination registers
        dest_fetch = new string[width];        dest_decode = new string[width];         dest_rename = new string[width];  
        dest_regread = new string[width];      dest_dispatch = new string[width];       dest_issue = new string[width];
        dest_execute = new string[width];      dest_writeback = new string[width*5];    dest_retire = new string[width];

        // Allocate for source registers
        src1_fetch = new string[width];        src1_decode = new string[width];         src1_rename = new string[width];   
        src1_regread = new string[width];      src1_dispatch = new string[width];       src1_issue = new string[width];    
        src1_execute = new string[width];      src1_writeback = new string[width*5];    src1_retire = new string[width];

        src2_fetch = new string[width];        src2_decode = new string[width];         src2_rename = new string[width];   
        src2_regread = new string[width];      src2_dispatch = new string[width];       src2_issue = new string[width];
        src2_execute = new string[width];      src2_writeback = new string[width*5];    src2_retire = new string[width];

        // Allocate for PC(address)
        pc_fetch = new string[width];     pc_decode = new string[width];        pc_rename = new string[width]; 
        pc_regread = new string[width];   pc_dispatch = new string[width];      pc_issue = new string[width];
        pc_execute = new string[width];   pc_writeback = new string[width*5];   pc_retire = new string[width];


        //this is not part of the design(probably,temp for human to read the instr. to debug)
        //Allocate for instr_num (instruction number)
        instr_num_fetch = new string[width];      instr_num_decode = new string[width];         instr_num_rename = new string[width]; 
        instr_num_regread = new string[width];    instr_num_dispatch = new string[width];       instr_num_issue = new string[width];
        instr_num_execute = new string[width];    instr_num_writeback = new string[width*5];    instr_num_retire = new string[width];
    
        //executing list
        executing_latency_counter = new int[width * 5];
        op_type_executing = new string[width * 5];
        dest_executing = new string[width * 5];
        src1_executing = new string[width * 5];
        src2_executing = new string[width * 5];
        pc_executing = new string[width * 5];
        instr_num_executing = new string[width * 5];

        //initalize to -1, means the exe list entry is free
        for(unsigned int i =0; i < width*5; i++){
             executing_latency_counter[i] = -1; 
        }
    }

    
    ~OOO_processor(){
        delete[] rob;
        delete[] rmt;
        delete[] queue;
        delete[] instruction;

        // Deallocate memory for op_type
        delete[] op_type_fetch;   delete[] op_type_decode;    delete[] op_type_rename;
        delete[] op_type_regread; delete[] op_type_dispatch;  delete[] op_type_issue;
        delete[] op_type_execute; delete[] op_type_writeback; delete[] op_type_retire;

        // Deallocate memory for destination registers
        delete[] dest_fetch;   delete[] dest_decode;    delete[] dest_rename;
        delete[] dest_regread; delete[] dest_dispatch;  delete[] dest_issue;
        delete[] dest_execute; delete[] dest_writeback; delete[] dest_retire;

        // Deallocate memory for source registers
        delete[] src1_fetch;   delete[] src1_decode;    delete[] src1_rename;
        delete[] src1_regread; delete[] src1_dispatch;  delete[] src1_issue;
        delete[] src1_execute; delete[] src1_writeback; delete[] src1_retire;

        delete[] src2_fetch;   delete[] src2_decode;    delete[] src2_rename;
        delete[] src2_regread; delete[] src2_dispatch;  delete[] src2_issue;
        delete[] src2_execute; delete[] src2_writeback; delete[] src2_retire;

        // Deallocate memory for PC
        delete[] pc_fetch;      delete[] pc_decode; delete[] pc_rename;     delete[] pc_regread; 
        delete[] pc_dispatch;   delete[] pc_issue;  delete[] pc_execute;    delete[] pc_writeback; delete[] pc_retire;

        // Deallocate memory for instr_num
        delete[] instr_num_fetch;    delete[] instr_num_decode;    delete[] instr_num_rename;
        delete[] instr_num_regread; delete[] instr_num_dispatch;  delete[] instr_num_issue;
        delete[] instr_num_execute;  delete[] instr_num_writeback; delete[] instr_num_retire;

        // Deallocate memory for executing list
        delete[] executing_latency_counter;
        delete[] op_type_executing;
        delete[] dest_executing;
        delete[] src1_executing;
        delete[] src2_executing;
        delete[] pc_executing;
        delete[] instr_num_executing;

    }

    void print_all_instructions_info(){
        for (uint64_t i = 0; i < dynamic_instr_count; ++i){
            if(instruction[i].get_op_type() != "-")
                instruction[i].print_instruction_info();
        }
    }  

    // Member functions for different stages
    void Retire() {

        if(is_debug_mode){
        cout << endl <<"#########################################################global_cycle:" << global_cycle << endl;
        //test:retired instruction in the ROB
        std::cout << std::setw(16) << "ROB Num" 
                << std::setw(8) << "Value" 
                << std::setw(10) << "Dest Reg" 
                << std::setw(8) << "Ready" 
                << std::setw(10) << "Exception" 
                << std::setw(13) << "Mispredict" 
                << std::setw(5) << "PC" 
                << std::endl;
        }

        //If retire, reset the rob entry and move the head pointer
        //Notice: th retire can up to WIDTH number of instr in the ROB
        for(unsigned long int j = 0; j < width; j++){
            if(ptr_head -> get_ready_bit() == 1){
                
                if(is_debug_mode){
                    //for testing/debugging -> to see which instr is/are retired
                    cout << endl <<"Retire:";// << endl;
                    std::cout << std::setw(8) << ptr_head -> get_rob_number()
                            << std::setw(8) << ptr_head -> get_value();
                    std::cout << setw(10) << ptr_head -> get_dest_reg(); 
                    std::cout << std::setw(8) << ptr_head -> get_ready_bit()
                        << std::setw(10) << ptr_head -> get_exception()
                        << std::setw(13) << ptr_head -> get_misprediction()
                        << std::setw(5) << ptr_head -> get_PC() << endl; 
                }

                //update RMT if needed
                //if the retire instr in the lastest version, then update RMT valid bit == 0, 
                //which means the ARF will have the lastest version/value of register(fyi, we dont actually apply ARF in this simulator)
                for(int i = 0; i < ARF_and_RMT_entries; i++){
                    if(rmt[i].get_rob_tag() == (ptr_head -> get_rob_number())){
                        rmt[i].set_valid_bit(0);
                        rmt[i].set_rob_tag("-");
                    }
                }

                //set up instruction timing info(Ended time)
                int pc_int = std::stoi(ptr_head -> get_PC().erase(0,1));
                instruction[pc_int].set_End_cycle(global_cycle + 1);
                
                //retire(clean the ROB entry)   
                ptr_head -> set_value(0);
                ptr_head -> set_dest_reg("-");
                ptr_head -> set_ready(0);
                ptr_head -> set_exception(0);
                ptr_head -> set_misprediction(0);
                ptr_head -> set_PC("-");

            
                //if reach the end of the rob, circle back
                if(ptr_head == (rob + rob_size - 1)) 
                    ptr_head = rob;
                else 
                    ptr_head = ptr_head + 1;

                num_of_retire_instr = num_of_retire_instr + 1;


            }else if (ptr_head -> get_ready_bit() == 0){
                break;
            }
        }

        if(is_check_boolean_mode){
            cout << "num_of_retire_instr:" << num_of_retire_instr << endl;
            cout << "dynamic_instr_count:" << dynamic_instr_count << endl;
        }
        


        if(num_of_retire_instr == (dynamic_instr_count))
            is_pipeline_empty = true;
        else 
            is_pipeline_empty = false;

        //First check if ROB has enough free entries for entire bundle(will double when reach the Rename stage)
        unsigned long int number_of_free_entries_in_ROB = 0;
        for(unsigned long int i = 0; i < rob_size; i++){
            if(rob[i].get_PC() == "-")//mean this entry is free
                number_of_free_entries_in_ROB = number_of_free_entries_in_ROB + 1;
        }
        if(number_of_free_entries_in_ROB >= width)
            has_ROB_free_entries = true;
        else
            has_ROB_free_entries = false;

        //this might cause bug if the ROB is full
        // if(ptr_head == ptr_tail)
        //     is_pipeline_empty = true;
        // else
        //     is_pipeline_empty = false;

    }

    //99.99%
    void Writeback(){

        if(is_debug_mode){
            //table title
            std:: cout << "       ";
            std::cout << std::setw(7) << "PC" 
            << std::setw(7) << "Op" 
            << std::setw(7) << "dest" 
            << std::setw(7) << "s1" 
            << std::setw(7) << "s2"
            << std::setw(8) << "latency"
            << std::endl;

            //number_of_instr_from_EX_stage = index;
            for (unsigned long int i = 0; i < number_of_instr_from_EX_stage; i++) {
                std::cout << "writeback" << i << ":";
                std::cout << std::setw(3) << pc_writeback[i];
                std::cout << std::setw(7) << op_type_writeback[i];
                std::cout << std::setw(7) << dest_writeback[i];
                std::cout << std::setw(7) << src1_writeback[i];
                std::cout << std::setw(7) << src2_writeback[i] << std::endl;
            }
        }

        // To maintain a non-blocking Execute stage, there is no
        // constraint on the number of instructions that may complete in a cycle.
        //set up ready bit for the entry in the ROB
        for (unsigned long int i = 0; i < number_of_instr_from_EX_stage; i++){

            if(pc_writeback[i] == "")
                return; //do nothing 

            //set up instruction timing info(timing when instr go to WB stage)
            int pc_int = std::stoi(pc_writeback[i].erase(0,1));
            pc_writeback[i] = "i" + std::to_string(pc_int); //put the "i" back for debugging purpose
            //instruction[pc_int].set_WB_cycle(global_cycle);
            instruction[pc_int].set_RT_cycle(global_cycle + 1); //also set up the RT cycle(since WB only last one cycle)

            //mark the instruction as "ready" in its entry in the ROB
            //find entry in the rob, assert ready bit
            for(unsigned long int k = 0; k < rob_size; k++){
                if(rob[k].get_rob_number() == dest_writeback[i]){
                    rob[k].set_ready(1);
                }
            }

            //we dont have to have clean the reg in WB stage I think since it only serves as a way to clean ROB
            //and the clean of ROB will be deal in the retire stage, so, not a big deal I think.
        }
    }

    //99.99%
    void Execute(){

        //print executing list
        if(is_debug_mode){
            for(unsigned long int i = 0; i < width * 5; i++) {
                //if(executing_latency_counter[i] != -1){
                    std::cout << "exe list" << i << ":";
                    std::cout << std::setw(4) << pc_executing[i];
                    std::cout << std::setw(7) << op_type_executing[i];
                    std::cout << std::setw(7) << dest_executing[i];
                    std::cout << std::setw(7) << src1_executing[i];
                    std::cout << std::setw(7) << src2_executing[i];// << std::endl;
                    std::cout << std::setw(8) << executing_latency_counter[i] << std::endl;
                //}
            }
        }
        
        //Check if the instruction are finished in this cycle, if so, then
        //correct sequence: 2->3->1(I chose this sequence) or 3->2->1 ???
        //1.remove the instr from the execution_listis_pipeline_empty (actually this might need to go last)
        //2.send the instr to the WB(writeback) stage (this step is in the writeback function)
        //3.wake up dependent instruction in the Issue Queue(IQ), Dispatch, and RegRead Stage
        unsigned long int index = 0;
        number_of_instr_from_EX_stage = 0;
        for(unsigned long int j = 0; j < (width * 5); j++){
            //step0:check the instr from the executing list that will be finished within this cycle
            if(executing_latency_counter[j] == 0){//Update:means the instr is finished in this cycle, wakeup the dependant instr and sent the instr to WB stage //Old:means this entry's instr will be finished the execution in this cycle(1->0, 0 means finished)

                //2.send instr to writeback stage(I think this must send before removing from the exe_list)
                pc_writeback[index] = pc_executing[j];
                op_type_writeback[index] = op_type_executing[j];
                dest_writeback[index] = dest_executing[j];
                src1_writeback[index] = src1_executing[j];
                src2_writeback[index] = src2_executing[j];
                index = index + 1;
                
                //set up instruction timing info(timing when instr send to WB stage)
                int pc_int = std::stoi(pc_executing[j].erase(0,1));
                pc_executing[j] = "i" + std::to_string(pc_int); //put the "i" back for debugging purpose
                instruction[pc_int].set_WB_cycle(global_cycle + 1);
                
                //3.wake up dependent instr in the Issue Queue(IQ), Dispatch, and RegRead Stage
                //wake up instr in IQ(set source ready bit
                for(unsigned long int k = 0; k < issue_queue_size; k++){
                    if(queue[k].get_rs1_tag_or_value() == dest_executing[j] && queue[k].get_valid_bit() == 1){
                        queue[k].set_rs1_ready_bit(1);
                        //queue[k].set_rs1_tag_or_value(queue[k].get_rs1_tag_or_value() + "(bypass)");
                        queue[k].set_rs1_tag_or_value("bypass");
                    }
                    if(queue[k].get_rs2_tag_or_value() == dest_executing[j] && queue[k].get_valid_bit() == 1){
                        queue[k].set_rs2_ready_bit(1);
                        //queue[k].set_rs2_tag_or_value(queue[k].get_rs2_tag_or_value() + "(bypass)");
                        queue[k].set_rs2_tag_or_value("bypass");
                    }
                }

                //wake up dispatch stage\//Just simply make the source register = "bypass" can have a wakeup effect for the instr
                //since if the source reg is not the rob(i.e.bypass and imm) will setup that instr's source reg's ready bit == 1
                //Even if it's rob, it will check the reorder buffer, if the rob is ready, the processor will also set up ready bit == 1, no worries
                for(unsigned long int i = 0; i < width; i++){
                    if(src1_dispatch[i] == dest_executing[j]){
                        src1_dispatch[i] = "bypass";
                    }
                    if(src2_dispatch[i] == dest_executing[j]){
                        src2_dispatch[i] = "bypass";
                    }
                }

                //wake up regread
                for(unsigned long int i = 0; i < width; i++){
                    if(src1_regread[i] == dest_executing[j]){
                        src1_regread[i] = "bypass";
                    }
                    if(src2_regread[i] == dest_executing[j]){
                        src2_regread[i] = "bypass";
                    }
                }

                //The following thought might be wrong since it can't pass validation8
                //debug: I think the processor also need to bypass the correct value to the rename stage
                //but based on my design, the registers in the rename stage is not renamed while 
                //this OOO processor is executed from the end, therefore, the processor need to 
                //1.check the RMT, see it the dest reg exist in the RMT, if RMT valid bit == 1(has speculate version)
                //2.then land its original register name, if this register name == the source registers in the rename register(buffer)
                //3.bypass the value to it
                // for(unsigned long int i = 0; i < width; i++){
                //     for(int k = 0; k < ARF_and_RMT_entries; k++){
                //         if(rmt[k].get_rob_tag() == dest_executing[j]){
                //             if(rmt[k].get_reg_number()== src1_rename[i]){
                //                 src1_rename[i] = "bypass";
                //             }
                //             if(rmt[k].get_reg_number() == src2_rename[i]){
                //                 src2_rename[i] = "bypass";
                //             }
                //         }
                //     }
                // }

                

                //1.remove that instr from the execution_list
                executing_latency_counter[j] = -1; // -1 means the exe list entry is free
                op_type_executing[j] = "";
                dest_executing[j] = "";
                src1_executing[j] = "";
                src2_executing[j] = "";
                pc_executing[j] = "";
                instr_num_executing[j] = "";

                number_of_instr_from_EX_stage = number_of_instr_from_EX_stage + 1;
            }
        }
    }

    //99%
    void Issue(){

        if(is_debug_mode){
            // //test:print out Issue Queue
            std::cout << "Issue Queue:" << endl;
            std::cout << "age | V | dst_tag | rs1_rdy | rs1_tag/value | rs2_rdy | rs2_tag/value | OpType\n";
            std::cout << "------------------------------------------------------------------------------\n";
            for (unsigned long int i = 0; i < issue_queue_size; i++) {
                // Print in table format with proper column alignment
                std::cout << std::setw(3) << queue[i].get_instr_age() << " | "
                        << std::setw(1) << queue[i].get_valid_bit() << " | "
                        << std::setw(7) << queue[i].get_dest_tag() << " | "
                        << std::setw(7) << queue[i].get_rs1_ready_bit() << " | "
                        << std::setw(13) << queue[i].get_rs1_tag_or_value() << " | "
                        << std::setw(7) << queue[i].get_rs2_ready_bit() << " | "
                        << std::setw(14) << queue[i].get_rs2_tag_or_value() << "|"
                        << std::setw(6) << queue[i].get_op_type() << "|\n";
            }
        }

        if(is_debug_mode){
            //test:table title
            std:: cout << "       ";
            std::cout << std::setw(7) << "PC" 
            << std::setw(7) << "Op" 
            << std::setw(7) << "dest" 
            << std::setw(7) << "s1" 
            << std::setw(7) << "s2"
            << std::endl;
        }

        //Step1:Issue up to WIDTH instr from issue queue to exe stage(execute list)
        /*algrithm: always start from the first instruction(i0),
        then check if issue queue has this instr by using age(since age == instr sequence)
        if the issue queue has that instr, check if it's valid and ready to be executed(both source register's ready bits == 1),
        if ready, send to exe stage, if not, go find the next oldest(the next small instr sequence),
        do it again.If there are more than WIDTH instr ready, the processor can only move WIDTH instructions
        to the exe stage(which will be the oldest four due to property of algorithm), the rest of ready instr
        need to wait in the queue(might be picked up in the cycle, no worries).
        If only n instructions are ready, n < WIDTH, the processor will also send these n instr to exe stage*/
        issue_instr_count = 0;
        for(unsigned long int i = 0; i < width; i++){
            for(int k = 0; k < total_instr_count; k++){
                for(unsigned long int m = 0; m < issue_queue_size; m++){
                    if((k == queue[m].get_instr_age()) && (queue[m].get_valid_bit() == 1)){ //check if the entry in queue is valid(check from oldest to youngest)
                            if((queue[m].get_rs1_ready_bit() == 1)  && (queue[m].get_rs2_ready_bit() == 1)){

                                //Add the instrunctions to the execute_list and set a timer
                                for(unsigned long int j = 0; j < (width * 5); j++){
                                    if(executing_latency_counter[j] == -1){//means this entry is empty(or say, there is no in-flight execution in this function unit
                                        //setup timer
                                        if(queue[m].get_op_type() == "0"){
                                            executing_latency_counter[j] = 1;
                                        }else if(queue[m].get_op_type() == "1"){
                                            executing_latency_counter[j] = 2;
                                        }else if(queue[m].get_op_type() == "2"){
                                            executing_latency_counter[j] = 5; // for debugging, slide value is 6, the correct one is 5(based on SPEC)
                                        }else{
                                            cout << "error in op type" << endl;
                                        }
                                        op_type_executing[j] = queue[m].get_op_type();
                                        dest_executing[j] = queue[m].get_dest_tag();
                                        src1_executing[j] = queue[m].get_rs1_tag_or_value();
                                        src2_executing[j] = queue[m].get_rs2_tag_or_value();
                                        pc_executing[j] = "i" + std::to_string(queue[m].get_instr_age());

                                        //set up instruction timing info
                                        int pc_int = std::stoi(pc_executing[j].erase(0,1));
                                        pc_executing[j] = "i" + std::to_string(pc_int); //put the "i" back for debugging purpose
                                        instruction[pc_int].set_EX_cycle(global_cycle + 1);
                                        break;
                                    }
                                }

                                //Remove the instr from the issue queue, and reset that issue queue entry
                                //set the age == -1 && valid bit == 0 means the issue queue entry is free(invalid)
                                queue[m].set_instr_age(-1);
                                queue[m].set_valid_bit(0);
                                queue[m].set_dest_tag("-");
                                queue[m].set_rs1_ready_bit(0);
                                queue[m].set_rs1_tag_or_value("-");
                                queue[m].set_rs2_ready_bit(0);
                                queue[m].set_rs2_tag_or_value("-");
                                queue[m].set_op_type("-");

                                issue_instr_count = issue_instr_count + 1;
                                if(issue_instr_count == width){
                                    goto check_if_IQ_enough; //exit all loop since the max number of instr can be sent is the size of a bundle which is width
                                }
                        }
                    }
                }
            }
        }
        

    check_if_IQ_enough:

        //check if Issue Queue has enough entries to accepty bundle from dispatch stage
        unsigned long int number_of_free_issue_queue_entries = 0;   
        for(unsigned long int i = 0; i < issue_queue_size; i++){
            if(queue[i].get_instr_age() == -1){ //means this entry is free(or can check valid bit == 0 actually)
                number_of_free_issue_queue_entries = number_of_free_issue_queue_entries + 1;
            }
        }
        if(number_of_free_issue_queue_entries >= width){
            is_IQ_enough = true;
        }else{
            is_IQ_enough = false;
        }
    }

    //99.99%
    void Dispatch(){

        if(is_check_boolean_mode)
            cout << "is_IQ_enough:" << is_IQ_enough << endl;

        if(is_debug_mode){
            //test:print instr
            for (unsigned long int i = 0; i < width; i++){
                std::cout << "dispatch" << i << ":" ;
                std::cout << std::setw(4) << pc_dispatch[i];
                std::cout << std::setw(7) << op_type_dispatch[i];
                std::cout << std::setw(7) << dest_dispatch[i];
                std::cout << std::setw(7) << src1_dispatch[i];
                std::cout << std::setw(7) << src2_dispatch[i] << std::endl;
            }
        }
        
        
        if(!is_IQ_enough){
            if(pc_dispatch[0] == "")
                is_DI_empty = true;
            else 
                is_DI_empty = false;
            return;
        }
        
        if(is_IQ_enough){ //if issue queue free entires > number of bundle(width)
            is_DI_empty = true;
            for(unsigned long int i = 0; i < width; i++){
                
                if(pc_dispatch[i] == "")
                    return;

                //set up instruction timing info
                int pc_int = std::stoi(pc_dispatch[i].erase(0,1));
                pc_dispatch[i] = "i" + std::to_string(pc_int); //put the "i" back for debugging purpose
                //instruction[pc_int].set_DI_cycle(global_cycle);
                //also setup the timing when the instr/bundle entering the IS stage 
                //since in the IS stage, we might only can know the time that instr/bundle leave the Issue Queue
                instruction[pc_int].set_IS_cycle(global_cycle + 1);

            
                //dispatch all instructions from DI to Issue Queue(IQ)
                for(unsigned long int j = 0; j < issue_queue_size; j++){
                    if(queue[j].get_instr_age() == -1){ //is age = -1, means the entry is free to used(or can used valid bit to detect actually)

                        //change assign the age to instr(fetch the int part in the string PC, i.e. i123 -> 123)
                        //set up the age in the issue queue(actually just the integer part of the human readable PC)
                        //then change pc_dispatch back to string(i.e. 123 -> i123)
                        int instr_age_int = stoi(pc_dispatch[i].erase(0, 1));
                        queue[j].set_instr_age(instr_age_int);
                        pc_dispatch[i] = "i" + std::to_string(instr_age_int); 

                        //set valid bit == 1 means the entry in the IQ is valid(need to be checked for issuing to exe stage(FUs))
                        queue[j].set_valid_bit(1);
                        queue[j].set_dest_tag(dest_dispatch[i]);
                        queue[j].set_rs1_tag_or_value(src1_dispatch[i]);
                        queue[j].set_rs2_tag_or_value(src2_dispatch[i]);


                        
                        //if it's not rob version, it could only be value(value read from the ROB which is READY),imm(immediate value), bypass(from WB stage), or reg in ARF(commited version, which is always read/available)
                        //So set the ready bit == 1.
                        //else if is a rob, check rob table, see if it's ready, set up based on the ready bit in the reorder buffer
                        bool is_src1_a_rob = (src1_dispatch[i].find("rob") != std::string::npos); //this is kind tricky, if src1_dispatch contain "rob" in the string, it will return true
                        if(!is_src1_a_rob) //not a rob, only could be imm, reg, wakeup(will be bypass) which means ready
                            queue[j].set_rs1_ready_bit(1);
                        else{
                            int rob_num = std::stoi(src1_dispatch[i].erase(0,3)); // i.e. rob23 -> 23
                            src1_dispatch[i] = "rob" + std::to_string(rob_num); //take rob string back: 23 -> rob23

                            if(rob[rob_num].get_ready_bit() == 1)
                                queue[j].set_rs1_ready_bit(1);
                            else
                                queue[j].set_rs1_ready_bit(0);
                        }
                        
                        //this is kind tricky, if src2_dispatch contain "rob" in the string, it will return true
                        bool is_src2_a_rob = (src2_dispatch[i].find("rob") != std::string::npos);
                        if(!is_src2_a_rob)
                            queue[j].set_rs2_ready_bit(1);
                        else{
                            int rob_num = std::stoi(src2_dispatch[i].erase(0,3)); // i.e. rob23 -> 23
                            src2_dispatch[i] = "rob" + std::to_string(rob_num); //take rob string back: 23 -> rob23

                            if(rob[rob_num].get_ready_bit() == 1)
                                queue[j].set_rs2_ready_bit(1);
                            else
                                queue[j].set_rs2_ready_bit(0);
                        }
                
                        //Additionally set up operation type to tell exe stage how long it will take to execute
                        queue[j].set_op_type(op_type_dispatch[i]);


                        //clean after sending instr to IQ(issue queue)
                        //clean the reg after sending to next stage
                        pc_dispatch[i] = "";
                        op_type_dispatch[i] = "";
                        dest_dispatch[i] = "";
                        src1_dispatch[i] = "";
                        src2_dispatch[i] = "";
                        
                        break; //return outter for loop after we place a instr.
                    }
                }
            }
        }
    }

    //99.99%
    void RegRead(){

        if(is_check_boolean_mode)
            cout << "is_DI_empty: " << is_DI_empty << endl;

        if(is_debug_mode){     
            for (unsigned long int i = 0; i < width; i++) {
                std::cout << "regread" << i << ":" ;
                std::cout << std::setw(5) << pc_regread[i];
                std::cout << std::setw(7) << op_type_regread[i];
                std::cout << std::setw(7) << dest_regread[i];
                std::cout << std::setw(7) << src1_regread[i];   
                std::cout << std::setw(7)  << src2_regread[i] << endl;            
            }
        }

        //update2:this for loop should be placed outside the if else condition, since the if the pipeline stall, 
        //        this state's register won't detect if it/they can land the value from the current instructions which is at the last cycle of EX stage 
        //update1:small query:is this should be done in the RN or RR? A:both
        //land the value from the ROB if the ROB is ready
        //this must be implemented in this stage because the processor might encounter 
        //the situation like, 
        //cycle N: the source reg in the ROB is ready, instr x is in the RR stage
        //cycle N+1: the srouce reg in the ROB is retire(first), then the ROB entry ready bit become 0
        //cycle N+1(continue): the instr X go to dispath, being placed in the issue Queue,
        //cycle N+1(continue): but at this cycle, the entry is retired but the instr can acutally land the value in the previous cycle
        //cycle N+1(continue): that whay the processor need to land the value from the rob at this stage(RR stage)
        for(unsigned long int i = 0; i < width; i++){
            for(unsigned long int j = 0; j < rob_size; j++){
                if(rob[j].get_rob_number() == src1_regread[i]){
                    if(rob[j].get_ready_bit() == 1){
                        src1_regread[i] = "value"; //we dont implement ARF here, set it as "value" will set the ready bit == 1 when dispatching the instr. to issue queue
                    }
                }
                if(rob[j].get_rob_number() == src2_regread[i]){
                    if(rob[j].get_ready_bit() == 1){
                        src2_regread[i] = "value"; //we dont implement ARF here, set it as "value" will set the ready bit == 1 when dispatching the instr. to issue queue
                    }
                }
            }
        }
        
        if(!is_DI_empty){
            if(pc_regread[0] == "")
                is_RR_empty = true;
            else
                is_RR_empty = false;
            return;
        }
        
        if(is_DI_empty){ 
            is_RR_empty = true;
            //Advance from RR to DI stage
            for(unsigned long int i = 0; i < width; i++){

                if(pc_regread[i] == ""){
                    pc_dispatch[i] = pc_regread[i];
                    op_type_dispatch[i] = op_type_regread[i];
                    dest_dispatch[i] = dest_regread[i];
                    src1_dispatch[i] = src1_regread[i];
                    src2_dispatch[i] = src2_regread[i];
                    return;
                }
                    
                //set up instruction timing info
                int pc_int = std::stoi(pc_regread[i].erase(0,1));
                pc_regread[i] = "i" + std::to_string(pc_int); //put the "i" back for debugging purpose
                instruction[pc_int].set_DI_cycle(global_cycle + 1);

                //advance the register value to next stage
                pc_dispatch[i] = pc_regread[i];
                op_type_dispatch[i] = op_type_regread[i];
                dest_dispatch[i] = dest_regread[i];
                src1_dispatch[i] = src1_regread[i];
                src2_dispatch[i] = src2_regread[i];
                //clean the reg after sending to next stage
                pc_regread[i] = "";
                op_type_regread[i] = "";
                dest_regread[i] = "";
                src1_regread[i] = "";
                src2_regread[i] = "";
            }
        }         
    }

    //99.99%
    void Rename(){ 

        //double check if ROB has enough free entries for entire bundle
        unsigned long int number_of_free_entries_in_ROB = 0;
        for(unsigned long int i = 0; i < rob_size; i++){
            if(rob[i].get_PC() == "-")//mean this entry is free
                number_of_free_entries_in_ROB = number_of_free_entries_in_ROB + 1;
        }
        if(number_of_free_entries_in_ROB >= width)
            has_ROB_free_entries = true;
        else
            has_ROB_free_entries = false;


        if(is_check_boolean_mode){
            cout << "is_RR_empty: " << is_RR_empty << endl;
            cout << "has_ROB_free_entries: " << has_ROB_free_entries << endl;
        }

        //update2:this for loop should be placed outside the if else condition, since the if the pipeline stall, 
        //        this state's register won't detect if it/they can land the value from the current instructions which is at the last cycle of EX stage 
        //update:small query:is this should be done in the RN or RR? A:both
        //land the value from the ROB if the ROB is ready
        //this must be implemented in this stage because the processor might encounter 
        //the situation like, 
        //cycle N: the source reg in the ROB is ready, instr x is in the RR stage
        //cycle N+1: the srouce reg in the ROB is retire(first), then the ROB entry ready bit become 0
        //cycle N+1(continue): the instr X go to dispath, being placed in the issue Queue,
        //cycle N+1(continue): but at this cycle, the entry is retired but the instr can acutally land the value in the previous cycle
        //cycle N+1(continue): that whay the processor need to land the value from the rob at this stage(RR stage)
        // for(unsigned long int i = 0; i < width; i++){
        //     for(unsigned long int j = 0; j < rob_size; j++){
        //         if(rob[j].get_rob_number() == src1_rename[i]){
        //             if(rob[j].get_ready_bit() == 1){
        //                 src1_rename[i] = "value"; //we dont implement ARF here, set it as "value" will set the ready bit == 1 when dispatching the instr. to issue queue
        //             }
        //         }
        //         if(rob[j].get_rob_number() == src2_rename[i]){
        //             if(rob[j].get_ready_bit() == 1){
        //                 src2_rename[i] = "value"; //we dont implement ARF here, set it as "value" will set the ready bit == 1 when dispatching the instr. to issue queue
        //             }
        //         }
        //     }
        // }
        
        if(is_debug_mode){
                for(unsigned long int i = 0; i < width; i++) {
                std:: cout << "rename" << i << ":" ;
                std::cout << std::setw(6) << pc_rename[i];    
                std::cout << std::setw(7) << std::dec << op_type_rename[i]; 
                std::cout << std::setw(7) << dest_rename[i];
                std::cout << std::setw(7) << src1_rename[i];
                std::cout << std::setw(7) << src2_rename[i] << endl;
            }
        }

        //if can't proceed to next stage, means the Rename stage has to stall(do nothing)
        //need to keep the bundle info in this stage
        //therefore, rename is not empty(is_RN_empty = false)
        if(!is_RR_empty || !has_ROB_free_entries){
            is_RN_empty = false;
            goto print_rename; //return;
        }
        
        //if RR is is empty, and ROB has free entry, 
        //then RN(rename) can recieve the instr from decode stage, 
        //also set the flag is_RN_empty = true to notify the decode stage can proceed the pipeline, then do the following
        // 1. allocate an entry in the ROB for Instr. 
        // 2. Rename source register
        // 3. Update RMT
        // 4. Rename dest. register(if any)
        // 5. Shift tail, pointing to next rob
        if(is_RR_empty && has_ROB_free_entries){
            is_RN_empty = true;
            for(unsigned long int i = 0; i < width; i++){
                
                // //tail == head, stall? but has ROB free entries might already deal with it
                // if()

                if(pc_rename[i] == ""){
                    pc_regread[i] = pc_rename[i];
                    op_type_regread[i] = op_type_rename[i];
                    dest_regread[i] = dest_rename[i];
                    src1_regread[i] = src1_rename[i];
                    src2_regread[i] = src2_rename[i];
                    goto print_rename;
                    break;
                }
                //step1:allocate an entry in the ROB for Instr.
                //this simulator doesn't implement misprediction, page falut, and ARF.
                //therefore, value, exception, and misprediction bit are not used.
                //ptr_tail->set_value(0); 
                ptr_tail->set_dest_reg(dest_rename[i]);          
                ptr_tail->set_ready(0); //ready bit = 0, the computation is not complete         
                //ptr_tail->set_exception(0);         
                //ptr_tail->set_misprediction(0);     
                ptr_tail->set_PC(pc_rename[i]);
                           
                
                //step2:Rename source register 1 in instruction(if needed)
                if(src1_rename[i] != "-"){//if has source register 1  
                    for(int j = 0; j < ARF_and_RMT_entries; j++){
                        if(rmt[j].get_reg_number() == src1_rename[i]){ //find the entry of RMT
                            if(rmt[j].get_rob_tag() != "-"){ //if there is rename(speculate) version of reg(rob)
                                //cout << "source reg1 before renaming:" << src1_rename[i] << endl; //test
                                src1_rename[i] = rmt[j].get_rob_tag(); //rmt's index = src1_rename[i] since src1_rename[i] record the real reg number which is exactly #rmt
                                //cout << "source reg1 after renaming:" << src1_rename[i] << endl; //test
                                break;
                            }
                        }
                    }   
                }
         
                //step2:Rename source register 2 in instruction(if needed)
                if(src2_rename[i] != "-"){//if has source register 1  
                    for(int j = 0; j < ARF_and_RMT_entries; j++){
                        if(rmt[j].get_reg_number() == src2_rename[i]){ //find the entry of RMT
                            if(rmt[j].get_rob_tag() != "-"){ //if there is rename(speculate) version of reg(rob)
                                //cout << "source reg2 before renaming:" << src2_rename[i]; //test
                                src2_rename[i] = rmt[j].get_rob_tag(); //rmt's index = src1_rename[i] since src1_rename[i] record the real reg number which is exactly #rmt
                                //cout << "source reg2 after renaming:" << src2_rename[i] << endl; //test
                                break;
                            }
                        }
                    }   
                }
            
                
                //step3:Update RMT        
                if(dest_rename[i] != "-"){ //if there is a destination register
                    for(int j = 0; j < ARF_and_RMT_entries; j++){ //search RMT entirely, and setup rob tag in RMT
                        if(rmt[j].get_reg_number() == dest_rename[i]){
                            rmt[j].set_rob_tag(ptr_tail -> get_rob_number());
                            rmt[j].set_valid_bit(1);
                            break;
                        }
                    }
                }
         

                //step4:Rename dest. reg         
                dest_rename[i] = ptr_tail -> get_rob_number(); //we set up dest register to indicate the instr.'s position in ROB entry even this instr. doesn't really have a destination but it will still occupy a position in ROB 
                //cout << dest_rename[i] << endl; //test


                //debug:
                //update:small query:is this should be done in the RN or RR? A:both
                //land the value from the ROB if the ROB is ready
                //this must be implemented in this stage because the processor might encounter 
                //the situation like, 
                //cycle N: the source reg in the ROB is ready, instr x is in the RR stage
                //cycle N+1: the srouce reg in the ROB is retire(first), then the ROB entry ready bit become 0
                //cycle N+1(continue): the instr X go to dispath, being placed in the issue Queue,
                //cycle N+1(continue): but at this cycle, the entry is retired but the instr can acutally land the value in the previous cycle
                //cycle N+1(continue): that whay the processor need to land the value from the rob at this stage(RR stage)
                for(unsigned long int j = 0; j < rob_size; j++){
                    if(rob[j].get_rob_number() == src1_rename[i]){
                        if(rob[j].get_ready_bit() == 1){
                            src1_rename[i] = "value"; //we dont implement ARF here, set it as "value" will set the ready bit == 1 when dispatching the instr. to issue queue
                        }
                    }
                    if(rob[j].get_rob_number() == src2_rename[i]){
                        if(rob[j].get_ready_bit() == 1){
                            src2_rename[i] = "value"; //we dont implement ARF here, set it as "value" will set the ready bit == 1 when dispatching the instr. to issue queue
                        }
                    }
                }

                //step5:advance from RN to RR (should advanced after renaming)
                pc_regread[i] = pc_rename[i];
                op_type_regread[i] = op_type_rename[i];
                dest_regread[i] = dest_rename[i];
                src1_regread[i] = src1_rename[i];
                src2_regread[i] = src2_rename[i];
                //clean the reg after sending to next stage
                pc_rename[i] = "";
                op_type_rename[i] = "";
                dest_rename[i] = "";
                src1_rename[i] = "";
                src2_rename[i] = "";

                //setup timing info
                int pc_int = std::stoi(pc_regread[i].erase(0,1));
                pc_regread[i] = "i" + std::to_string(pc_int); //put the "i" back for debugging purpose
                instruction[pc_int].set_RR_cycle(global_cycle + 1);

                //step5: shift tail
                if(ptr_tail == (rob + rob_size - 1)) // -1 since rob start from 0
                    ptr_tail = rob;
                else
                    ptr_tail = ptr_tail + 1;
            }
        }

    print_rename:
        
        if(is_debug_mode){    
            //print RMT table
            std::cout << endl <<"Rename Map Table (RMT)" << std::endl;
            // Print the column headers
            std::cout << std::setw(10) << "reg_num" 
                    << std::setw(5)  << "V" 
                    << std::setw(10) << "rob_tag" 
                    << std::endl;
            // Print a horizontal separator
            std::cout << std::string(25, '-') << std::endl;
            // Iterate through the RMT entries and print their details
            for (int i = 0; i < ARF_and_RMT_entries; i++) {
                std::cout << std::setw(9) << rmt[i].get_reg_number()   // reg_num
                        << std::setw(6) << rmt[i].get_valid_bit()           // valid bit (V)
                        << std::setw(7);
                std::cout << rmt[i].get_rob_tag();
                std::cout << std::endl;
            }

            //print Reorder buffer
            //Print table header
            cout << endl <<"Reorder buffer:" << endl;
            std::cout << std::setw(8) << "ROB Num" 
                    << std::setw(8) << "Value" 
                    << std::setw(10) << "Dest Reg" 
                    << std::setw(8) << "Ready" 
                    << std::setw(10) << "Exception" 
                    << std::setw(13) << "Mispredict" 
                    << std::setw(5) << "PC" 
                    << std::endl;

            //Print a horizontal separator
            std::cout << std::string(62, '-') << std::endl;
            //Iterate through the ROB entries and print their details
            for (unsigned long int i = 0; i < rob_size; i++) {
                if((ptr_head -> get_rob_number() == rob[i].get_rob_number()) && (ptr_tail -> get_rob_number() == rob[i].get_rob_number())){
                    string label1 = "HT" + rob[i].get_rob_number();
                    std::cout <<std::setw(8) << label1;
                }else if(ptr_head -> get_rob_number() == rob[i].get_rob_number()){
                    string label2 = "H" + rob[i].get_rob_number();
                    std::cout <<std::setw(8) << label2;
                }else if(ptr_tail -> get_rob_number() == rob[i].get_rob_number()){
                    string label3 = "T" + rob[i].get_rob_number();
                    std::cout <<std::setw(8) << label3;
                }else
                    std::cout <<std::setw(8) << rob[i].get_rob_number();

                std::cout << std::setw(8) << rob[i].get_value();
                std::cout << setw(10) << rob[i].get_dest_reg(); 
                std::cout << std::setw(8) << rob[i].get_ready_bit()
                        << std::setw(10) << rob[i].get_exception()
                        << std::setw(13) << rob[i].get_misprediction()
                        << std::setw(5) << rob[i].get_PC() << endl; 
            }
        }
    }

    //99.99%
    void Decode(){

        if(is_check_boolean_mode)
            cout << "is_RN_empty: " << is_RN_empty << endl;


        if(is_debug_mode){
            std:: cout << endl << "       ";
            std::cout << std::setw(7) << "PC" 
            << std::setw(7) << "Op" 
            << std::setw(7) << "dt" //destination 
            << std::setw(7) << "s1" 
            << std::setw(7) << "s2" 
            << std::endl;
            for (unsigned long int i = 0; i < width; i++) {
                std:: cout << "decode" << i << ":" ;
                std::cout << std::setw(6) << pc_decode[i]    // Print pc_decode in hexadecimal
                        << std::setw(7) << std::dec << op_type_decode[i] // Print op_type_decode in decimal
                        << std::setw(7) << dest_decode[i]              // Print dest_decode
                        << std::setw(7) << src1_decode[i]              // Print src1_decode
                        << std::setw(7) << src2_decode[i]              // Print src2_decode
                        << std::endl;
            }
        }

        //if RN is not empty, then decode stage can't send the bundle to RN stage(do nothing), 
        //therefore, it's not empty -> is_DE_empty = false
        if(!is_RN_empty){
            is_DE_empty = false; 
            return;//do nothing
        }

        

        //Advance decode stage to rename
        if(is_RN_empty){
            is_DE_empty = true; //tell Fetch stage that DE stage can accept new bundle
            for(unsigned long int i = 0; i < width; i++){
                //if there is no new instr then exit the loop
                if(pc_decode[i] == ""){
                    //propgate empty stuff to next cycle so that we can stop the pipeline(stop renaming)
                    pc_rename[i] = pc_decode[i];
                    op_type_rename[i] = op_type_decode[i];
                    dest_rename[i] = dest_decode[i];
                    src1_rename[i] = src1_decode[i];
                    src2_rename[i] = src2_decode[i];
                    return; //do nothing //goto print_decode;
                }

                //advance decode to rename stage
                pc_rename[i] = pc_decode[i];
                op_type_rename[i] = op_type_decode[i];
                dest_rename[i] = dest_decode[i];
                src1_rename[i] = src1_decode[i];
                src2_rename[i] = src2_decode[i];
                //clean the reg after sending to next stage
                pc_decode[i] = "";
                op_type_decode[i] = "";
                dest_decode[i] = "";
                src1_decode[i] = "";
                src2_decode[i] = "";

                //set up instruction timing info
                int pc_int = std::stoi(pc_rename[i].erase(0,1));
                pc_rename[i] = "i" + std::to_string(pc_int); //put the "i" back for debugging purpose
                instruction[pc_int].set_RN_cycle(global_cycle + 1);
            }
        }
    }

    //99.99%
    void Fetch(FILE *FP) {

        //test
        if(is_check_boolean_mode){
            cout << "is_DE_empty: " << is_DE_empty << endl;
        }
        

        if(is_trace_depleted || !is_DE_empty){
            goto print_fetch; //do nothing
        }

        //fetch the number of width instructions from the trace file(superscalar fetch multiple instr. at a time)
        //width = bundle size = number of instruction in a bundle
        if(!is_trace_depleted && is_DE_empty){
            for(unsigned long int i = 0; i < width; i++){
                fscanf(FP, "%lx %d %d %d %d", &pc, &op_type, &dest, &src1, &src2);

                //check if the processor already read all the instruction in the trace file
                //notice, if the processor exactly read the last line in the trace file, it might detect as "false"
                //in other word, it will only set false when it read the (blank)
                if(feof(FP)){
                    is_trace_depleted = true;
                    op_type_fetch[i] = "";
                    pc_fetch[i] = "";
                    dest_fetch[i] = "";
                    src1_fetch[i] = "";
                    src2_fetch[i] = "";
                    //try to propagate the empty stuff to the next pipeline stage so they can know the tracefile is emtpy, 
                    //no need for furthur action
                    pc_decode[i] = pc_fetch[i];
                    op_type_decode[i] = op_type_fetch[i];
                    dest_decode[i] = dest_fetch[i];
                    src1_decode[i] = src1_fetch[i];
                    src2_decode[i] = src2_fetch[i];
                    goto print_fetch;
                    break;
                }else{
                    is_trace_depleted = false;
                }
                
                //test: this is for human to trace the PC eaiser might be print out in the end
                pc_fetch[i] = "i" + std::to_string(dynamic_instr_count);
                dynamic_instr_count = dynamic_instr_count + 1;
                //pc_fetch[i] = pc; //The actually PC is not used

                op_type_fetch[i] = std::to_string(op_type);


                if(dest != -1) //if there is a destination reg
                    dest_fetch[i] = "r" + std::to_string(dest);
                else//(dest == -1)if there is no destination reg(i.e.branch)
                    dest_fetch[i] = "-";
                
                
                if (src1 != -1) // if there is a source register 1
                    src1_fetch[i] = "r" + std::to_string(src1);
                else  // (src1 == -1) if there is no source register 1, i.e.immediate value
                    src1_fetch[i] = "imm";


                if (src2 != -1) // if there is a source register 2
                    src2_fetch[i] = "r" + std::to_string(src2);
                else  // (src2 == -1) if there is no source register 2
                    src2_fetch[i] = "imm";

                //set up timing info(the time when instruction entering the FE stage)
                //step1:get the integer part of the PC, then, use this int to point to the *instruction for setting up the timing info
                //cout << "test int1:" << pc_fetch[i] << endl;
                int pc_int = std::stoi(pc_fetch[i].erase(0,1));
                pc_fetch[i] = "i" + std::to_string(pc_int); //put the "i" back for debugging purpose
                instruction[pc_int].set_FE_cycle(global_cycle);
                instruction[pc_int].set_op_type(op_type_fetch[i]);
                instruction[pc_int].set_src1(std::to_string(src1));
                instruction[pc_int].set_src2(std::to_string(src2));
                instruction[pc_int].set_dst(std::to_string(dest));
                //cout << "test int11:" << pc_fetch[i] << endl;

                //send the fetched instr to DE stage
                pc_decode[i] = pc_fetch[i];
                op_type_decode[i] = op_type_fetch[i];
                dest_decode[i] = dest_fetch[i];
                src1_decode[i] = src1_fetch[i];
                src2_decode[i] = src2_fetch[i];
                //clean the reg after sending to next stage
                pc_fetch[i] = "";
                op_type_fetch[i] = "";
                dest_fetch[i] = "";
                src1_fetch[i] = "";
                src2_fetch[i] = "";

                //set up timing info
                //cout << "test int2:" << pc_decode[i] << endl;
                int pc_int2 = std::stoi(pc_decode[i].erase(0,1));
                pc_decode[i] = "i" + std::to_string(pc_int2); //put the "i" back for debugging purpose
                instruction[pc_int2].set_DE_cycle(global_cycle + 1);
                //cout << "test int2:" << pc_decode[i] << endl;

            }   
        }
       
        
        print_fetch:

        //since the fetch stage's registers are clean after the sending to decode stage
        //therefore, the valid register value would be the ones in decode stage
        if(is_debug_mode){
            if(!is_trace_depleted && is_DE_empty){
                for(unsigned long int i = 0; i < width; i++){
                    std:: cout << "fetch" << i << ":" ;
                    std::cout << std::setw(7) << pc_decode[i]    
                            << std::setw(7) << std::dec << op_type_decode[i] 
                            << std::setw(7) << dest_decode[i]              
                            << std::setw(7) << src1_decode[i]              
                            << std::setw(7) << src2_decode[i]           
                            << std::endl;
                }
            }else if(is_trace_depleted || !is_DE_empty){
                for(unsigned long int i = 0; i < width; i++){
                    std:: cout << "fetch" << i << ":" ;
                    std::cout << std::setw(7) << pc_fetch[i]    
                            << std::setw(7) << std::dec << op_type_fetch[i] 
                            << std::setw(7) << dest_fetch[i]              
                            << std::setw(7) << src1_fetch[i]              
                            << std::setw(7) << src2_fetch[i]           
                            << std::endl;
                }
            }
        }
    }

    //Done
    /*The function has two function
    1. Advance_the_cycle
    2. track if the program is finished*/
    bool Advanced_Cycle() {
        bool is_program_finished = false;

        global_cycle += 1;      

        //count down the cycle for the execution in the function unit
        for(unsigned long int j = 0; j < (width * 5); j++){
            if(executing_latency_counter[j] > 0){
                executing_latency_counter[j] = executing_latency_counter[j] - 1;
            }
        }
        
        is_program_finished = is_pipeline_empty & is_trace_depleted;

        //is_program_finished = is_trace_depleted;// & is_trace_depleted;

        if(is_check_boolean_mode){
            cout << "is_pipeline_empty:" << is_pipeline_empty << endl;
            cout << "is_trace_depleted:" << is_trace_depleted << endl;
            cout << "is_program_finished:" << is_program_finished << endl;
        }
        
        if(is_debug_mode_counter){
            if(debug_counter > 0)
                debug_counter = debug_counter - 1;
            else 
                debug_counter = debug_counter;

            if(debug_counter == 0)
                is_program_finished = true;
        }
        return is_program_finished;
    }

    void display_results(){
        //IPC = dynamic_instr_count / global_cycle;

        double IPC = 0.0;
        if (global_cycle > 0) { 
            IPC = static_cast<double>(dynamic_instr_count) / global_cycle; // Explicit type cast for clarity
        } else {
            IPC = 0.0;
        }
        cout << "# === Simulation Results ========" << endl;
        cout << "# Dynamic Instruction Count   = " << dynamic_instr_count << endl;
        cout << "# Cycles                      = " << global_cycle << endl;
        cout << std::fixed << std::setprecision(2);
        cout << "# Instructions Per Cycle (IPC) = " << IPC << endl;
    }

};

/* =========== The following the are main function =========== */

/*  argc holds the number of command line arguments
    argv[] holds the commands themselves

    Example:-
    sim 256 32 4 gcc_trace.txt
    argc = 5
    argv[0] = "sim"
    argv[1] = "256"
    argv[2] = "32"
    ... and so on
*/
int main (int argc, char* argv[])
{   
    //test: for easy terminal read 

    //cout << endl << "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~" << endl << endl << endl << endl << endl;

    FILE *FP;               // File handler
    char *trace_file;       // Variable that holds trace file name;
    proc_params params;       // look at sim_bp.h header file for the the definition of struct proc_params
    // int op_type, dest, src1, src2;  // Variables are read from trace file
    // uint64_t pc; // Variable holds the pc read from input file
    
    if (argc != 5)
    {
        printf("Error: Wrong number of inputs:%d\n", argc-1);
        exit(EXIT_FAILURE);
    }

    // argument read like this: ./sim 256 32 4 gcc_trace.txt
    params.rob_size     = strtoul(argv[1], NULL, 10); //in the above example: rob size = 256
    params.iq_size      = strtoul(argv[2], NULL, 10); //iq size: 32
    params.width        = strtoul(argv[3], NULL, 10); //(instruction width):4 (I think this is like the size fo the bundle, each bundle might contain 4 instruction for example) 
    trace_file          = argv[4];
    // printf("rob_size:%lu "
    //         "iq_size:%lu "
    //         "width:%lu "
    //         "tracefile:%s\n", params.rob_size, params.iq_size, params.width, trace_file);
    // Open trace_file in read mode
    FP = fopen(trace_file, "r");
    if(FP == NULL)
    {
        // Throw error and exit if fopen() failed
        printf("Error: Unable to open file %s\n", trace_file);
        exit(EXIT_FAILURE);
    }

    
    
    ///////////////////////////////////////////////////////////////////////////////////////////////////////
    //
    // The following loop just tests reading the trace and echoing it back to the screen.
    //
    // Replace this loop with the "do { } while (Advance_Cycle());" loop indicated in the Project 3 spec.
    // Note: fscanf() calls -- to obtain a fetch bundle worth of instructions from the trace -- should be
    // inside the Fetch() function.
    //
    //while(fscanf(FP, "%lx %d %d %d %d", &pc, &op_type, &dest, &src1, &src2) != EOF) //
    //    printf("%lx %d %d %d %d\n", pc, op_type, dest, src1, src2); //Print to check if inputs have been read correctly
    //
    ///////////////////////////////////////////////////////////////////////////////////////////////////////
    OOO_processor processor(params.rob_size, params.iq_size, params.width); //instantiate my processor
    bool is_program_finished = true;

    do{
        
        processor.Retire();
        processor.Writeback();
        processor.Execute();
        processor.Issue();
        processor.Dispatch();
        processor.RegRead();
        processor.Rename();
        processor.Decode();
        processor.Fetch(FP);
        is_program_finished = processor.Advanced_Cycle();

        //test:
        //cout << "is_program_finished: " << is_program_finished << endl;
        //is_program_finished = true;

    }while(!is_program_finished);

    processor.print_all_instructions_info();
    

    //Output
    cout << "# === Simulator Command =========" << endl;
    cout << "# " << argv[0] << " " << params.rob_size << " " << params.iq_size << " " << params.width << " " << trace_file << endl; //argv[0] is the "./sim" in this case

    cout << "# === Processor Configuration ===" << endl;
    cout << "# ROB_SIZE   = " << params.rob_size << endl;
    cout << "# IQ_SIZE    = " << params.iq_size << endl;
    cout << "# WIDTH      = " << params.width << endl;
    // << "tracefile: " << trace_file << std::endl;

    processor.display_results();


    return 0;
}


