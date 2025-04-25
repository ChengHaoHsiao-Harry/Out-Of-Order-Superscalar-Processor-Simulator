# Out-Of-Order-Superscalar-Processor-Simulator
This is a simulator to simulate an OOO superscalar processor for dynamic instructions scheduling and latency calculation
This simulator focus on the dynamic scheduling(i.e., assume perfect branch predictor -> no recovery for branch misprediction and perfect cache -> no cache miss to get the instruction from lower-level I cache or data from the lower-level D cache.)
<img width="1576" alt="image" src="https://github.com/user-attachments/assets/fd70ea84-ef50-4515-8322-409193ed3661" />



1. Type "make" to build.  (Type "make clean" first if you already compiled and want to recompile from scratch.)

2. Run trace reader:

   format:
   ./sim <ROB_SIZE> <IQ_SIZE> <Bundle_WIDTH> <trace_file>

   Parameters:
   ROB_SIZE: Number of entries in the Reorder Buffer

   IQ_SIZE: Number of entries in the Issue Queue
   
   Bundle_WIDTH: Superscalar width (max instructions per cycle)
   
   trace_file: Input trace file path

   To run without throttling output:
   ./sim 256 32 4 gcc_trace.txt

   To run with throttling (via "less"):
   ./sim 256 32 4 gcc_trace.txt | less

4. Trace file:

   instruction format:
   <PC> <operation_type> <dest_reg> <src1_reg> <src2_reg>
   
   instruction example(-1 counld mean immediate value or no destionation register like branch):
   2b642c 2 19 30 -1


5. Output Format:

   The simulator provides:
   
   1.Per-instruction timing information:
     <seq_no> fu{<op_type>} src{<src1>,<src2>} dst{<dst>} FE{<begin-cycle>,<duration>} DE{<begin-cycle>,<duration>} RN{…} RR{…} DI{…} IS{…} EX{…} WB{…} RT{…}
     example:
	original instruction: 2b642c 2 19 30 -1
     output timing info: 1 fu{2} src{30,-1} dst{19} FE{0,1} DE{1,1} RN{2,1} RR{3,1} DI{…} IS{…} EX{…} WB{…} RT{…}
	*"1" mean PC 2b642c is the first instruction in the program

   2.Final statistics including:
     Dynamic instruction count
     Total cycles
     Instructions per cycle (IPC)

6. In the program:
   The following variable is for debugging purpose, set the debug mode = 1 can view each pipline's instruction, ROB, Issue Queue, and RMT.
   
   Set "is_check_boolean_mode" = 1 can view if pipeline need to stall.
   
   Reader can ignore the debug counter as it's for designer to debug at the very begining stage.
   
   bool is_debug_mode = 0;
   bool is_check_boolean_mode = 0;
   bool is_debug_mode_counter = 0;
   int debug_counter = 110;

   
