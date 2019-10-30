# CS130 Project 2: User Program

## Group 20 Members

- Yuqing Yao yaoyq@shanghaitech.edu.cn
- Yiwei Yang yangyw@shanghaitech.edu.cn

## Task 1: Argument Passing

In the implementation of Args Passing, I commented the project1 task2 and task3 part which may trigger list.h vaddr assertion and modify to the optimized variable settings. And in the DisignDoc, I'll not cover the structs or functions I've commented.

### Data Structure 

#### Edited structs

##### `struct thread`

- `struct file *code_file`

  To store the executable code files. Once the process is `load()`ed, code_file will be loaded simultaneously. When `process_exit()`ed, lock the writes.

#### Edited Functions

- `process_execute (const char *file_name)`
  
  When executing the process, code_file should be processed.
  
- `load (const char *file_name, void (**eip) (void), void **esp)`

  When loading the file, code_file should be processed.

- `setup_stack (void **esp, char * file_name)`

  The funtion is to map a zeroed page at the top of vaddr, in the func, we have to realize string split and other pre-processing.

- `void process_exit (void)`

  When exiting the process, code_file should be processed.
#### Algorithm

Address | Name | Data | Type 
0xbfffff | ---
LearnShare | 12
Mike |  32